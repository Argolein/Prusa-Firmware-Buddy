#include "speed_model_runtime.hpp"
#include "calibration_verify.hpp"
#include "diagnostics.hpp"
#include "phase_stepping.hpp"
#include "speed_model.hpp"

#include <heap.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

namespace phase_stepping::speed_model_runtime {

namespace {

    struct FreeTable {
        void operator()(speed_model::HarmonicTable *table) const {
            free(table);
        }
    };
    using TablePtr = std::unique_ptr<speed_model::HarmonicTable, FreeTable>;

    /// The stock allocator redscreens when out of memory; a table is optional.
    TablePtr allocate_table() {
        void *memory = malloc_fallible(sizeof(speed_model::HarmonicTable));
        return TablePtr(memory ? new (memory) speed_model::HarmonicTable : nullptr);
    }

    /// [motor][direction], direction 0 = forward.
    std::array<std::array<speed_model::DirectionRuntime, 2>, opts::SUPPORTED_AXIS_COUNT> runtimes;
    std::array<std::array<std::array<TablePtr, speed_model::MAX_MODELLED_HARMONICS>, 2>, opts::SUPPORTED_AXIS_COUNT> tables;

    CorrectedCurrentLut &lut_of(AxisEnum axis, int direction) {
        return direction == 0 ? axis_states[axis].forward_current : axis_states[axis].backward_current;
    }

} // namespace

void deactivate(AxisEnum axis) {
    for (int direction = 0; direction < 2; direction++) {
        CorrectedCurrentLut &lut = lut_of(axis, direction);
        // The ISR preempts this task and never runs alongside it, so once the
        // pointer is cleared no refresh reads the tables any more.
        lut._speed_model = nullptr;
        std::atomic_signal_fence(std::memory_order_seq_cst);
        lut.set_excluded_harmonics(0);
        for (auto &table : tables[axis][direction]) {
            table.reset();
        }
        runtimes[axis][direction].count = 0;
    }
}

void apply(AxisEnum axis) {
    deactivate(axis);
    if (!diagnostics::speed_model_enabled()) {
        return;
    }
    const calibration_verify::MotorVerification &record = calibration_verify::record(axis);
    for (int direction = 0; direction < 2; direction++) {
        speed_model::DirectionRuntime &runtime = runtimes[axis][direction];
        CorrectedCurrentLut &lut = lut_of(axis, direction);
        uint32_t excluded = 0;
        int n = 0;
        for (int harmonic = 1; harmonic <= opts::CORRECTION_HARMONICS && n < speed_model::MAX_MODELLED_HARMONICS; harmonic++) {
            const calibration_verify::HarmonicVerification &item = record.direction[direction][harmonic];
            if (!item.kept || !item.speed_model.active) {
                continue;
            }
            TablePtr &table = tables[axis][direction][n];
            table = allocate_table();
            if (!table) {
                continue; // out of memory: this harmonic stays in the summed table
            }
            speed_model::build_harmonic_table(*table, harmonic, lut.get_correction_table()[harmonic]);
            runtime.harmonic[n] = harmonic;
            runtime.model[n] = item.speed_model;
            runtime.table[n] = table.get();
            excluded |= 1u << harmonic;
            n++;
        }
        if (n == 0) {
            continue;
        }
        runtime.count = n;
        runtime.last_speed = speed_model::DirectionRuntime::FORCE_RECOMPUTE;
        runtime.recompute(0);
        runtime.last_speed = speed_model::DirectionRuntime::FORCE_RECOMPUTE;
        // Momentarily without the harmonics rather than applying them twice.
        lut.set_excluded_harmonics(excluded);
        std::atomic_signal_fence(std::memory_order_seq_cst);
        lut._speed_model = &runtime;
    }
}

} // namespace phase_stepping::speed_model_runtime
