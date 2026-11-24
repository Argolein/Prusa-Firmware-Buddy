#pragma once

#include "common.hpp"
#include "lut.hpp"

#include <array>
#include <cstdint>

/// Speed-dependent correction of calibrated harmonics (INDX only).
///
/// A harmonic with an active model is not part of the summed correction
/// table. It has its own table, built from its stored size and phase, which
/// is read at an index offset that follows the motor speed:
/// pha(v) = pha(v0) - phase_offset(|v|) and size(v) = size(v0) * size_factor(|v|).
/// Everything here is free of firmware headers so the host tests build it.
namespace phase_stepping::speed_model {

/// Speeds are motor speeds in rev/s.
inline constexpr float RECOMPUTE_STEP = 0.05f;
/// The driver current is lowered from this speed up, which the model cannot see.
inline constexpr float HOLD_SPEED = 6.f;
inline constexpr float MIN_SIZE_FACTOR = 0.5f;
inline constexpr float MAX_SIZE_FACTOR = 1.5f;
inline constexpr float MAX_CHANGE_CLAMP = 1.5f;
inline constexpr int GAIN_FRACTIONAL = 8;
/// Calibration models harmonics 2 and 4 only.
inline constexpr int MAX_MODELLED_HARMONICS = 2;

/// Speed model of one harmonic and direction, as calibrated and persisted.
struct HarmonicModel {
    bool active = false;
    /// Calibration speed of the stored size and phase.
    float v0 = 0;
    /// rad per rev/s.
    float slope = 0;
    /// Standard error of `slope`.
    float slope_se = 0;
    /// 1 per rev/s.
    float size_slope = 0;
    /// Highest measured speed.
    float band_top = 0;
    /// Largest measured phase change from v0, rad.
    float max_change = 0;
};

/// Phase decrease in rad at speed @p abs_speed. Linear up to band_top; above,
/// the slope is scaled by clamp(1 - 2 slope_se / |slope|, 0, 1); constant from
/// HOLD_SPEED up; magnitude clamped to MAX_CHANGE_CLAMP * max_change.
float phase_offset(const HarmonicModel &model, float abs_speed);

/// Size factor at speed @p abs_speed, held above band_top, within
/// [MIN_SIZE_FACTOR, MAX_SIZE_FACTOR].
float size_factor(const HarmonicModel &model, float abs_speed);

/// Index offset into a harmonic table realizing a phase decrease @p phase_offset.
int table_offset(float phase_offset, int harmonic);

/// One period of a single harmonic, then the same period again, so a table
/// read at `start + idx` with start in the first period never wraps.
using HarmonicTable = std::array<int8_t, 2 * opts::MOTOR_PERIOD>;

/// Fill @p table with harmonic @p harmonic of size and phase @p item alone, in
/// the units and rounding of the summed table, saturated to int8.
void build_harmonic_table(HarmonicTable &table, int harmonic, const SpectralItem &item);

/// Runtime state of one motor and direction, read by the phase stepping ISR
/// through `CorrectedCurrentLut::_speed_model`. Filled while unpublished; once
/// published, only recompute() (from the ISR) writes it.
struct DirectionRuntime {
    /// Modelled harmonics in use, 1 or 2 while published.
    uint8_t count = 0;
    /// Every gain is exactly 1, so the refresh skips the multiplications.
    bool unity_gain = true;
    std::array<uint8_t, MAX_MODELLED_HARMONICS> harmonic {};
    std::array<HarmonicModel, MAX_MODELLED_HARMONICS> model {};
    std::array<const HarmonicTable *, MAX_MODELLED_HARMONICS> table {};
    /// Table read position for motor phase 0 at the current speed.
    std::array<const int8_t *, MAX_MODELLED_HARMONICS> start {};
    /// Size factor, GAIN_FRACTIONAL fixed point.
    std::array<int, MAX_MODELLED_HARMONICS> gain {};
    float last_speed = FORCE_RECOMPUTE;

    static constexpr float FORCE_RECOMPUTE = -1e6f;

    /// Recompute start positions and gains for @p abs_speed. Kept out of line
    /// so the common refresh does not pay for its registers.
    [[gnu::noinline, gnu::cold]] void recompute(float abs_speed);

    /// Phase shift at motor phase @p idx: the base table (without the
    /// modelled harmonics) plus each modelled harmonic at its speed offset,
    /// saturated like the summed table.
    [[gnu::always_inline]] int phase_shift(const CorrectedCurrentLut &base, int idx, float abs_speed) {
        if (__builtin_fabsf(abs_speed - last_speed) > RECOMPUTE_STEP) [[unlikely]] {
            recompute(abs_speed);
        }
        idx &= opts::MOTOR_PERIOD - 1;
        int shift = base._phase_shift[idx];
        if (unity_gain) [[likely]] {
            shift += start[0][idx];
            if (count > 1) {
                shift += start[1][idx];
            }
        } else {
            for (int k = 0; k < count; k++) {
                shift += (gain[k] * start[k][idx] + (1 << (GAIN_FRACTIONAL - 1))) >> GAIN_FRACTIONAL;
            }
        }
        // Not std::clamp: it is not inlined at the firmware's optimization level.
        return shift > 127 ? 127 : (shift < -127 ? -127 : shift);
    }

    [[gnu::always_inline]] CoilCurrents current(const CorrectedCurrentLut &base, int idx, float abs_speed) {
        const int pha = opts::SIN_FRACTION * idx + phase_shift(base, idx, abs_speed);
        return { sin_lut(pha, opts::CURRENT_AMPLITUDE), cos_lut(pha, opts::CURRENT_AMPLITUDE) };
    }
};

} // namespace phase_stepping::speed_model
