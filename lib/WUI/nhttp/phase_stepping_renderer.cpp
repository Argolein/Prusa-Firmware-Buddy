#include "phase_stepping_renderer.h"

#include <segmented_json_macros.h>

#include <device/cmsis.h>
#include <feature/phase_stepping/diagnostics.hpp>
#include <feature/phase_stepping/phase_stepping.hpp>

#include <cmath>
#include <cstdio>

namespace nhttp::link_content {

using namespace json;

namespace {

    constexpr uint8_t MOTOR_COUNT = 2;
    constexpr uint8_t DIRECTION_COUNT = 2;

    constexpr AxisEnum motor_axis(uint8_t motor) {
        return motor == 0 ? X_AXIS : Y_AXIS;
    }

    using Number = std::array<char, 16>;

    // `%.6g` keeps 6 significant digits for the small magnitudes; non-finite
    // values (e.g. a corrupted table) become `null` to keep the JSON valid.
    Number format_number(float value) {
        Number out {};
        if (std::isfinite(value)) {
            snprintf(out.data(), out.size(), "%.6g", static_cast<double>(value));
        } else {
            snprintf(out.data(), out.size(), "null");
        }
        return out;
    }

} // namespace

PhaseSteppingRenderState::PhaseSteppingRenderState() {
    namespace diag = phase_stepping::diagnostics;
    speed_model_enabled = diag::speed_model_enabled();
    const diag::IsrTiming isr = diag::isr_timing();
    isr_max_cycles = isr.max_cycles;
    isr_mean_cycles = isr.mean_cycles;
    isr_samples = isr.samples;
    cpu_hz = SystemCoreClock;
    const diag::CalibrationHeap heap = diag::last_calibration_heap();
    heap_valid = heap.valid;
    heap_min_free_bytes = heap.min_free_bytes;
    heap_free_chunks_at_min = heap.free_chunks_at_min;
}

void PhaseSteppingRenderState::load_motor() {
    const AxisEnum axis = motor_axis(motor);
    motor_enabled = phase_stepping::axis_states[axis].enabled;
    motor_missed_tx = phase_stepping::diagnostics::missed_tx_total(axis);
}

void PhaseSteppingRenderState::load_spectrum() {
    const auto &axis_state = phase_stepping::axis_states[motor_axis(motor)];
    const auto &lut = direction == 0 ? axis_state.forward_current : axis_state.backward_current;
    spectrum = lut.get_correction_table();
}

void PhaseSteppingRenderState::load_pair() {
    const phase_stepping::SpectralItem &item = spectrum[harmonic];
    snprintf(pair.data(), pair.size(), "[%s,%s]", format_number(item.mag).data(), format_number(item.pha).data());
}

JsonResult PhaseSteppingRenderer::renderState(size_t resume_point, JsonOutput &output, PhaseSteppingRenderState &state) const {
    // clang-format off
    JSON_START;
    JSON_OBJ_START;
        JSON_FIELD_BOOL("speed_model_enabled", state.speed_model_enabled) JSON_COMMA;
        JSON_FIELD_OBJ("isr");
            JSON_FIELD_INT("max_cycles", state.isr_max_cycles) JSON_COMMA;
            JSON_FIELD_INT("mean_cycles", state.isr_mean_cycles) JSON_COMMA;
            JSON_FIELD_INT("samples", state.isr_samples) JSON_COMMA;
            JSON_FIELD_INT("cpu_hz", state.cpu_hz);
        JSON_OBJ_END JSON_COMMA;
        JSON_FIELD_OBJ("calibration_heap");
            JSON_FIELD_BOOL("valid", state.heap_valid) JSON_COMMA;
            JSON_FIELD_INT("min_free_bytes", state.heap_min_free_bytes) JSON_COMMA;
            JSON_FIELD_INT("free_chunks_at_min", state.heap_free_chunks_at_min);
        JSON_OBJ_END JSON_COMMA;
        JSON_FIELD_ARR("motors");
            for (; state.motor < MOTOR_COUNT; state.motor++) {
                if (state.motor != 0) {
                    JSON_COMMA;
                }
                state.load_motor();
                JSON_OBJ_START;
                    JSON_FIELD_STR("axis", state.motor == 0 ? "x" : "y") JSON_COMMA;
                    JSON_FIELD_STR("motor", state.motor == 0 ? "A" : "B") JSON_COMMA;
                    JSON_FIELD_BOOL("enabled", state.motor_enabled) JSON_COMMA;
                    JSON_FIELD_INT("missed_tx", state.motor_missed_tx);
                    for (; state.direction < DIRECTION_COUNT; state.direction++) {
                        JSON_COMMA;
                        state.load_spectrum();
                        JSON_FIELD_ARR(state.direction == 0 ? "forward" : "backward");
                        for (; state.harmonic < state.spectrum.size(); state.harmonic++) {
                            if (state.harmonic != 0) {
                                JSON_COMMA;
                            }
                            state.load_pair();
                            JSON_CUSTOM("%s", state.pair.data());
                        }
                        state.harmonic = 0;
                        JSON_ARR_END;
                    }
                    state.direction = 0;
                JSON_OBJ_END;
            }
        JSON_ARR_END;
    JSON_OBJ_END;
    JSON_END;
    // clang-format on
}

} // namespace nhttp::link_content
