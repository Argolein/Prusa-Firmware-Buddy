#pragma once

#include "common.hpp"

#include <array>
#include <cmath>

/// Check of the phase stepping calibration without recalibrating (`M976`,
/// INDX only): the residual of each kept harmonic is measured again at its
/// calibration speed and compared with the baseline stored by the calibration.
/// The result lives in RAM only. Free of firmware headers so the host tests
/// build it.
namespace phase_stepping::calibration_check {

/// A ratio residual(now) / baseline above this suggests recalibrating
/// (belt tension change, motor swap).
inline constexpr float RECALIBRATION_RATIO = 2.0f;

struct HarmonicCheck {
    /// The harmonic was measured by the check.
    bool checked = false;
    /// residual(now) / baseline; NaN when there is no baseline or the move failed.
    float ratio = NAN;
};

struct MotorCheck {
    /// [direction][harmonic], direction 0 = forward.
    std::array<std::array<HarmonicCheck, opts::CORRECTION_HARMONICS + 1>, 2> direction {};
};

struct CheckResult {
    /// The check completed and produced at least one ratio.
    bool valid = false;
    bool suggest_recalibration = false;
    /// 0 = X / A, 1 = Y / B
    std::array<MotorCheck, 2> motor {};
};

/// residual(now) / baseline. NaN when @p baseline is missing (not finite or
/// not positive) or @p now is not finite.
float residual_ratio(float now, float baseline);

/// Fill in `valid` and `suggest_recalibration` of @p result from its ratios.
/// @p completed is false when a measurement failed; such a check is never valid.
void conclude(CheckResult &result, bool completed);

/// Result of the last check since boot; `valid` is false when none ran.
/// Other tasks may read a partially updated result.
const CheckResult &last();

/// Store @p result as the last check.
void set_last(const CheckResult &result);

} // namespace phase_stepping::calibration_check
