#pragma once

#include <core/types.h>

#include <cstdint>

/// Runtime diagnostics of phase stepping, read by the PrusaLink dump.
///
/// Values are written from the phase stepping ISR or the calibration task and
/// read from the web server task. Each value is individually atomic; a reader
/// may see fields of one struct from slightly different moments.
namespace phase_stepping::diagnostics {

/// Cost of one phase stepping refresh in CPU cycles (DWT cycle counter).
struct IsrTiming {
    uint32_t max_cycles;
    /// Exponential moving average over roughly the last 1024 refreshes.
    uint32_t mean_cycles;
    uint32_t samples;
};

IsrTiming isr_timing();
void reset_isr_timing();
/// Called from the ISR once per refresh.
void record_isr_cycles(uint32_t cycles);

/// Free heap at its lowest point during the last calibration.
struct CalibrationHeap {
    /// False until a calibration has sampled the heap since boot.
    bool valid;
    uint32_t min_free_bytes;
    /// Number of free chunks (`mallinfo().ordblks`) at that moment, a fragmentation hint.
    uint32_t free_chunks_at_min;
};

CalibrationHeap last_calibration_heap();
/// Call when a calibration starts.
void begin_calibration_heap();
/// Call where all sample buffers of a calibration move are alive.
void sample_calibration_heap();

/// Missed TMC transmissions of one motor since boot.
uint32_t missed_tx_total(AxisEnum axis);
/// Called from the ISR each time a transmission is missed.
void record_missed_tx(AxisEnum axis);

/// Runtime switch for the speed model (`M975`). On after boot, not persisted.
bool speed_model_enabled();
void set_speed_model_enabled(bool enabled);

} // namespace phase_stepping::diagnostics
