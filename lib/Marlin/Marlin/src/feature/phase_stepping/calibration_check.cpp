#include "calibration_check.hpp"

namespace phase_stepping::calibration_check {

namespace {

    CheckResult last_result;

} // namespace

float residual_ratio(float now, float baseline) {
    if (!std::isfinite(now) || !std::isfinite(baseline) || !(baseline > 0)) {
        return NAN;
    }
    return now / baseline;
}

void conclude(CheckResult &result, bool completed) {
    bool any_ratio = false;
    bool suggest = false;
    for (const MotorCheck &motor : result.motor) {
        for (const auto &direction : motor.direction) {
            for (const HarmonicCheck &item : direction) {
                if (item.checked && std::isfinite(item.ratio)) {
                    any_ratio = true;
                    suggest = suggest || item.ratio > RECALIBRATION_RATIO;
                }
            }
        }
    }
    result.valid = completed && any_ratio;
    result.suggest_recalibration = result.valid && suggest;
}

const CheckResult &last() {
    return last_result;
}

void set_last(const CheckResult &result) {
    // Readers in other tasks treat the result as absent until `valid` is set.
    last_result.valid = false;
    last_result.motor = result.motor;
    last_result.suggest_recalibration = result.suggest_recalibration;
    last_result.valid = result.valid;
}

} // namespace phase_stepping::calibration_check
