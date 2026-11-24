#include "diagnostics.hpp"

#include <FreeRTOS.h>
#include <malloc.h>

#include <atomic>

namespace phase_stepping::diagnostics {

namespace {

    // Fixed-point shift of the moving average; also its window (2^10 refreshes).
    constexpr uint32_t MEAN_SHIFT = 10;

    std::atomic<uint32_t> max_cycles { 0 };
    std::atomic<uint32_t> mean_cycles_scaled { 0 };
    std::atomic<uint32_t> samples { 0 };

    std::atomic<bool> heap_valid { false };
    std::atomic<uint32_t> heap_min_free { 0 };
    std::atomic<uint32_t> heap_free_chunks_at_min { 0 };

    std::atomic<uint32_t> missed_tx[2] = { 0, 0 };

    std::atomic<bool> speed_model { true };

} // namespace

IsrTiming isr_timing() {
    return {
        .max_cycles = max_cycles.load(std::memory_order_relaxed),
        .mean_cycles = mean_cycles_scaled.load(std::memory_order_relaxed) >> MEAN_SHIFT,
        .samples = samples.load(std::memory_order_relaxed),
    };
}

void reset_isr_timing() {
    max_cycles.store(0, std::memory_order_relaxed);
    mean_cycles_scaled.store(0, std::memory_order_relaxed);
    samples.store(0, std::memory_order_relaxed);
}

void record_isr_cycles(uint32_t cycles) {
    // Only the ISR writes these, so plain load/store pairs are enough.
    if (cycles > max_cycles.load(std::memory_order_relaxed)) {
        max_cycles.store(cycles, std::memory_order_relaxed);
    }
    const uint32_t n = samples.load(std::memory_order_relaxed);
    uint32_t mean = mean_cycles_scaled.load(std::memory_order_relaxed);
    if (n == 0) {
        mean = cycles << MEAN_SHIFT;
    } else {
        mean = mean - (mean >> MEAN_SHIFT) + cycles;
    }
    mean_cycles_scaled.store(mean, std::memory_order_relaxed);
    samples.store(n + 1, std::memory_order_relaxed);
}

CalibrationHeap last_calibration_heap() {
    return {
        .valid = heap_valid.load(std::memory_order_relaxed),
        .min_free_bytes = heap_min_free.load(std::memory_order_relaxed),
        .free_chunks_at_min = heap_free_chunks_at_min.load(std::memory_order_relaxed),
    };
}

void begin_calibration_heap() {
    heap_valid.store(false, std::memory_order_relaxed);
    heap_min_free.store(UINT32_MAX, std::memory_order_relaxed);
    heap_free_chunks_at_min.store(0, std::memory_order_relaxed);
}

void sample_calibration_heap() {
    const uint32_t free_bytes = xPortGetFreeHeapSize();
    if (free_bytes < heap_min_free.load(std::memory_order_relaxed)) {
        heap_min_free.store(free_bytes, std::memory_order_relaxed);
        heap_free_chunks_at_min.store(mallinfo().ordblks, std::memory_order_relaxed);
    }
    heap_valid.store(true, std::memory_order_relaxed);
}

uint32_t missed_tx_total(AxisEnum axis) {
    return axis < 2 ? missed_tx[axis].load(std::memory_order_relaxed) : 0;
}

void record_missed_tx(AxisEnum axis) {
    if (axis < 2) {
        missed_tx[axis].fetch_add(1, std::memory_order_relaxed);
    }
}

bool speed_model_enabled() {
    return speed_model.load(std::memory_order_relaxed);
}

void set_speed_model_enabled(bool enabled) {
    speed_model.store(enabled, std::memory_order_relaxed);
}

} // namespace phase_stepping::diagnostics
