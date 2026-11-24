#include "speed_model.hpp"
#include "round_fixed.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace phase_stepping::speed_model {

namespace {

    // Rounds half away from zero without a libm call (used from the ISR).
    int round_to_int(float value) {
        return value >= 0 ? static_cast<int>(value + 0.5f) : -static_cast<int>(0.5f - value);
    }

} // namespace

float phase_offset(const HarmonicModel &model, float abs_speed) {
    const float v = std::min(abs_speed, HOLD_SPEED);
    float offset;
    if (v <= model.band_top) {
        offset = model.slope * (v - model.v0);
    } else {
        const float abs_slope = std::fabs(model.slope);
        const float c = abs_slope > 0 ? std::clamp(1 - 2 * model.slope_se / abs_slope, 0.f, 1.f) : 0.f;
        offset = model.slope * (model.band_top - model.v0) + c * model.slope * (v - model.band_top);
    }
    const float limit = MAX_CHANGE_CLAMP * model.max_change;
    return std::clamp(offset, -limit, limit);
}

float size_factor(const HarmonicModel &model, float abs_speed) {
    const float v = std::min(abs_speed, model.band_top);
    return std::clamp(1 + model.size_slope * (v - model.v0), MIN_SIZE_FACTOR, MAX_SIZE_FACTOR);
}

int table_offset(float phase_offset, int harmonic) {
    return round_to_int(phase_offset * opts::MOTOR_PERIOD / (2 * std::numbers::pi_v<float> * harmonic));
}

void build_harmonic_table(HarmonicTable &table, int harmonic, const SpectralItem &item) {
    const int mag = mag_to_fixed(item.mag);
    const int pha = pha_to_fixed(item.pha);
    for (int i = 0; i != opts::MOTOR_PERIOD; i++) {
        const int64_t raw = static_cast<int64_t>(mag) * sin_lut(harmonic * i * opts::SIN_FRACTION + pha);
        table[i] = std::clamp<int64_t>(round_fixed(raw, opts::SIN_LUT_FRACTIONAL + opts::MAG_FRACTIONAL), -127, 127);
    }
    std::copy_n(table.begin(), opts::MOTOR_PERIOD, table.begin() + opts::MOTOR_PERIOD);
}

void DirectionRuntime::recompute(float abs_speed) {
    last_speed = abs_speed;
    bool unity = true;
    for (int k = 0; k < count; k++) {
        const int offset = table_offset(phase_offset(model[k], abs_speed), harmonic[k]);
        // Reading table[(idx - offset) mod period] is reading start[idx].
        start[k] = table[k]->data() + ((-offset) & (opts::MOTOR_PERIOD - 1));
        gain[k] = round_to_int(size_factor(model[k], abs_speed) * (1 << GAIN_FRACTIONAL));
        unity = unity && gain[k] == (1 << GAIN_FRACTIONAL);
    }
    unity_gain = unity;
}

} // namespace phase_stepping::speed_model
