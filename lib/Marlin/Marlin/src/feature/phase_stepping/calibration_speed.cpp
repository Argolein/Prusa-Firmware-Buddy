#include "calibration_speed.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace phase_stepping::calibration_speed {

namespace {

    constexpr float TWO_PI = 2 * std::numbers::pi_v<float>;

} // namespace

std::optional<HarmonicPlan> plan_for(int harmonic) {
    switch (harmonic) {
    case 2:
        return HarmonicPlan { .band_limit = 3.2f, .default_speeds = { 1.f, 2.f, 3.f }, .fit_size = false };
    case 4:
        return HarmonicPlan { .band_limit = 1.6f, .default_speeds = { 0.5f, 1.f, 1.5f }, .fit_size = true };
    default:
        return std::nullopt;
    }
}

float alias(float freq, float sampling_freq) {
    const float half = sampling_freq / 2;
    float folded = std::fmod(freq + half, sampling_freq);
    if (folded < 0) {
        folded += sampling_freq;
    }
    return std::fabs(folded - half);
}

bool is_alias_free(float speed, int harmonic, float sampling_freq, int periods_per_rev) {
    const float electrical = speed * periods_per_rev;
    const float target = alias(harmonic * electrical, sampling_freq);
    for (int m = 1; m <= MAX_ALIAS_HARMONIC; m++) {
        if (m != harmonic && std::fabs(alias(m * electrical, sampling_freq) - target) < ALIAS_MARGIN_HZ) {
            return false;
        }
    }
    return true;
}

float select_speed(float target, int harmonic, float sampling_freq, int periods_per_rev, float max_speed) {
    for (int step = 0; step <= MAX_SPEED_STEPS; step++) {
        for (int sign : { 1, -1 }) {
            const float speed = target + sign * step * SPEED_STEP;
            if (speed > 0 && speed <= max_speed && is_alias_free(speed, harmonic, sampling_freq, periods_per_rev)) {
                return speed;
            }
            if (step == 0) {
                break;
            }
        }
    }
    return NAN;
}

float capture_move_revs(float speed, float duration, float settle_time, float accel) {
    return speed * (duration + 2 * settle_time) + speed * speed / accel + MOVE_MARGIN_REVS;
}

float capped_sweep_duration(float speed, float duration, float max_revs, float settle_time, float accel) {
    const float fixed_revs = capture_move_revs(speed, 0, settle_time, accel);
    const float capped = std::min(duration, (max_revs - fixed_revs) / speed);
    return (capped >= duration || capped * speed >= MIN_SWEEP_REVS) ? capped : NAN;
}

float unwrap_near(float phase, float reference) {
    return phase - TWO_PI * std::round((phase - reference) / TWO_PI);
}

PhaseFit fit_phase(std::span<const float> speeds, std::span<const float> phases, float v0) {
    const size_t n = std::min(speeds.size(), phases.size());
    if (n == 0) {
        return { .p0 = 0, .slope = 0, .slope_se = NAN };
    }
    float mean_x = 0, mean_y = 0;
    for (size_t i = 0; i < n; i++) {
        mean_x += speeds[i] - v0;
        mean_y += phases[i];
    }
    mean_x /= n;
    mean_y /= n;
    float sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; i++) {
        const float dx = speeds[i] - v0 - mean_x;
        sxx += dx * dx;
        sxy += dx * (phases[i] - mean_y);
    }
    if (n < 2 || sxx <= 0) {
        return { .p0 = mean_y, .slope = 0, .slope_se = NAN };
    }
    // phase = p0 - slope * x
    const float slope = -sxy / sxx;
    const float p0 = mean_y + slope * mean_x;
    float rss = 0;
    for (size_t i = 0; i < n; i++) {
        const float r = phases[i] - (p0 - slope * (speeds[i] - v0));
        rss += r * r;
    }
    const float slope_se = n < 3 ? NAN : std::sqrt(rss / (n - 2) / sxx);
    return { .p0 = p0, .slope = slope, .slope_se = slope_se };
}

float fit_size_slope(std::span<const float> speeds, std::span<const float> sizes, float v0, float size0) {
    const size_t n = std::min(speeds.size(), sizes.size());
    float sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; i++) {
        const float dx = speeds[i] - v0;
        sxx += dx * dx;
        sxy += dx * (sizes[i] / size0 - 1);
    }
    return (sxx > 0 && size0 > 0) ? sxy / sxx : 0.f;
}

float clamp_size_slope(float size_slope, float v0, float bottom, float top) {
    const float reach = std::max(std::fabs(top - v0), std::fabs(bottom - v0));
    if (reach <= 0) {
        return 0;
    }
    const float limit = (speed_model::MAX_SIZE_FACTOR - 1) / reach;
    static_assert(speed_model::MAX_SIZE_FACTOR - 1 == 1 - speed_model::MIN_SIZE_FACTOR);
    return std::clamp(size_slope, -limit, limit);
}

bool slope_exceeds_spread(float slope, float bottom, float top, float spread) {
    return std::fabs(slope) * (top - bottom) > 2 * spread;
}

SweepFit fit_sweep(std::span<const float> speeds, std::span<const float> phases, float v0, float reference_phase) {
    SweepFit result {};
    const size_t n = std::min({ speeds.size(), phases.size(), result.phases.size() });
    float previous = reference_phase;
    for (size_t i = 0; i < n; i++) {
        result.phases[i] = unwrap_near(phases[i], previous);
        previous = result.phases[i];
    }
    const std::span<const float> unwrapped(result.phases.data(), n);
    const PhaseFit fit = fit_phase(speeds.first(n), unwrapped, v0);

    result.spread = 0;
    for (size_t i = 0; i + REPEATS <= n; i += REPEATS) {
        const auto [lo, hi] = std::minmax_element(unwrapped.begin() + i, unwrapped.begin() + i + REPEATS);
        result.spread = std::max(result.spread, *hi - *lo);
    }
    float max_change = 0;
    for (float phase : unwrapped) {
        max_change = std::max(max_change, std::fabs(phase - fit.p0));
    }
    result.band_bottom = n ? *std::min_element(speeds.begin(), speeds.begin() + n) : 0.f;
    const float band_top = n ? *std::max_element(speeds.begin(), speeds.begin() + n) : 0.f;
    result.model = {
        .active = false,
        .v0 = v0,
        .slope = fit.slope,
        .slope_se = fit.slope_se,
        .size_slope = 0,
        .band_top = band_top,
        .max_change = max_change,
    };
    result.significant = n >= 3 && std::isfinite(fit.slope_se)
        && slope_exceeds_spread(fit.slope, result.band_bottom, band_top, result.spread);
    return result;
}

} // namespace phase_stepping::calibration_speed
