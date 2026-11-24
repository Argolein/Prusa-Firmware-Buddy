#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <calibration_speed.hpp>
#include <speed_model.hpp>

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <vector>

using namespace phase_stepping;
using namespace phase_stepping::calibration_speed;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float PI = std::numbers::pi_v<float>;
constexpr int PERIODS_PER_REV = 100;

const std::vector<float> SWEEP_SPEEDS { 1.f, 1.f, 2.f, 2.f, 3.f, 3.f };
// Alternating noise of known size: +-0.02 rad.
const std::vector<float> NOISE { 0.02f, -0.02f, -0.02f, 0.02f, 0.02f, -0.02f };

std::vector<float> line(float p0, float slope, float v0, bool wrap) {
    std::vector<float> phases;
    for (size_t i = 0; i != SWEEP_SPEEDS.size(); i++) {
        float phase = p0 - slope * (SWEEP_SPEEDS[i] - v0) + NOISE[i];
        if (wrap) {
            phase = std::remainder(phase, 2 * PI);
        }
        phases.push_back(phase);
    }
    return phases;
}

speed_model::HarmonicModel model(float v0, float slope, float slope_se, float band_top, float max_change) {
    return { .active = true, .v0 = v0, .slope = slope, .slope_se = slope_se, .size_slope = 0, .band_top = band_top, .max_change = max_change };
}

CorrectedCurrentLut make_lut(const MotorPhaseCorrection &spectrum, uint32_t excluded) {
    CorrectedCurrentLut lut;
    lut._excluded_harmonics = excluded;
    lut.modify_correction_table([&](MotorPhaseCorrection &table) {
        table = spectrum;
    });
    return lut;
}

} // namespace

TEST_CASE("Sweep speed selection avoids aliases") {
    // At 1600 Hz, harmonic 16 of 0.8 rev/s (1280 Hz) folds exactly onto harmonic 4 (320 Hz).
    CHECK_FALSE(is_alias_free(0.8f, 4, 1600, PERIODS_PER_REV));
    const float moved = select_speed(0.8f, 4, 1600, PERIODS_PER_REV, 1.6f);
    CHECK(std::isfinite(moved));
    CHECK(moved != 0.8f);
    CHECK(std::fabs(moved - 0.8f) <= MAX_SPEED_STEPS * SPEED_STEP + 1e-4f);
    CHECK(is_alias_free(moved, 4, 1600, PERIODS_PER_REV));

    CHECK(is_alias_free(1.5f, 4, 1600, PERIODS_PER_REV));
    CHECK(select_speed(1.5f, 4, 1600, PERIODS_PER_REV, 1.6f) == 1.5f);

    CHECK_THAT(alias(1280, 1600), WithinAbs(320, 1e-3));
    CHECK_THAT(alias(320, 1600), WithinAbs(320, 1e-3));
}

TEST_CASE("Phase fit recovers slope and standard error") {
    const float v0 = 0.9f, slope = 0.4f;
    const auto phases = line(1.f, slope, v0, false);
    const PhaseFit fit = fit_phase(SWEEP_SPEEDS, phases, v0);
    // The noise averages out at each speed, so the line is exact and the
    // residuals are the noise: se = sqrt(6 * 0.02^2 / (6 - 2) / sxx), sxx = 4.
    CHECK_THAT(fit.slope, WithinAbs(slope, 1e-4));
    CHECK_THAT(fit.p0, WithinAbs(1.f, 1e-4));
    CHECK_THAT(fit.slope_se, WithinAbs(std::sqrt(6 * 0.02f * 0.02f / 4 / 4), 1e-5));

    SECTION("Unwrapping across 2 pi") {
        // Rising phase that crosses +pi and wraps to negative values.
        const auto wrapped = line(2.8f, -1.2f, v0, true);
        const SweepFit sweep = fit_sweep(SWEEP_SPEEDS, wrapped, v0, 2.8f);
        CHECK_THAT(sweep.model.slope, WithinAbs(-1.2f, 0.03));
        CHECK_THAT(sweep.spread, WithinAbs(0.04f, 1e-3));
        CHECK(sweep.significant);
        CHECK(sweep.model.band_top == 3.f);
        CHECK(sweep.band_bottom == 1.f);
    }

    SECTION("Size slope") {
        const float size0 = 0.03f, g = 0.2f;
        std::vector<float> sizes;
        for (size_t i = 0; i != SWEEP_SPEEDS.size(); i++) {
            sizes.push_back(size0 * (1 + g * (SWEEP_SPEEDS[i] - v0)) * (1 + NOISE[i]));
        }
        CHECK_THAT(fit_size_slope(SWEEP_SPEEDS, sizes, v0, size0), WithinAbs(g, 0.02));
        // Factor must stay within [0.5, 1.5] from 1 to 3 rev/s.
        const float limit = 0.5f / (3.f - v0);
        CHECK_THAT(clamp_size_slope(2.f, v0, 1.f, 3.f), WithinAbs(limit, 1e-6));
        CHECK_THAT(clamp_size_slope(-2.f, v0, 1.f, 3.f), WithinAbs(-limit, 1e-6));
        CHECK(clamp_size_slope(0.1f, v0, 1.f, 3.f) == 0.1f);
    }
}

TEST_CASE("Self-gate keeps flat data inactive") {
    // Slope 0.01 rad per rev/s predicts 0.02 rad across the band, below twice the 0.04 spread.
    const auto phases = line(0.5f, 0.01f, 0.9f, false);
    const SweepFit sweep = fit_sweep(SWEEP_SPEEDS, phases, 0.9f, 0.5f);
    CHECK_FALSE(sweep.significant);
    CHECK_FALSE(slope_exceeds_spread(0.01f, 1.f, 3.f, 0.04f));
    CHECK(slope_exceeds_spread(0.1f, 1.f, 3.f, 0.04f));
}

TEST_CASE("Phase offset extrapolation") {
    using speed_model::phase_offset;
    const auto m = model(1.f, 0.5f, 0.1f, 2.f, 10.f);
    CHECK_THAT(phase_offset(m, 1.5f), WithinAbs(0.25f, 1e-6));
    CHECK_THAT(phase_offset(m, 0.f), WithinAbs(-0.5f, 1e-6));
    // Above band_top the slope is scaled by c = 1 - 2 * 0.1 / 0.5 = 0.6.
    CHECK_THAT(phase_offset(m, 3.f), WithinAbs(0.5f + 0.6f * 0.5f, 1e-5));
    // Held from 6 rev/s up.
    CHECK_THAT(phase_offset(m, 6.f), WithinAbs(0.5f + 0.6f * 0.5f * 4, 1e-5));
    CHECK(phase_offset(m, 9.f) == phase_offset(m, 6.f));
    // c = 0 when the error is at least half the slope.
    CHECK_THAT(phase_offset(model(1.f, 0.5f, 0.3f, 2.f, 10.f), 4.f), WithinAbs(0.5f, 1e-6));
    // Clamped to 1.5 times the largest measured change.
    CHECK_THAT(phase_offset(model(1.f, 0.5f, 0.1f, 2.f, 0.4f), 3.f), WithinAbs(0.6f, 1e-6));
    CHECK_THAT(phase_offset(model(1.f, -0.5f, 0.1f, 2.f, 0.4f), 3.f), WithinAbs(-0.6f, 1e-6));
}

TEST_CASE("Speed model tables") {
    MotorPhaseCorrection spectrum {};
    spectrum[2] = { 0.05f, 0.3f };
    spectrum[4] = { 0.03f, 1.1f };
    const CorrectedCurrentLut stock = make_lut(spectrum, 0);

    SECTION("No active harmonic: summed table is the stock one") {
        CorrectedCurrentLut lut = make_lut(spectrum, 1u << 4);
        lut.set_excluded_harmonics(0);
        CHECK(lut._phase_shift == stock._phase_shift);
    }

    speed_model::HarmonicTable table;
    speed_model::build_harmonic_table(table, 4, spectrum[4]);
    speed_model::DirectionRuntime runtime;
    runtime.harmonic[0] = 4;
    runtime.table[0] = &table;
    runtime.count = 1;
    const CorrectedCurrentLut base = make_lut(spectrum, 1u << 4);

    SECTION("Base plus harmonic at offset 0 equals the stock table") {
        runtime.model[0] = model(1.f, 0.f, 0.f, 3.f, 1.f);
        for (int i = 0; i != opts::MOTOR_PERIOD; i++) {
            INFO("idx " << i);
            CHECK(std::abs(runtime.phase_shift(base, i, 1.f) - stock._phase_shift[i]) <= 1);
        }
    }

    SECTION("Offset sign: the phase decreases by slope * (v - v0)") {
        // 20 table steps of harmonic 4, an exact table offset.
        const float decrease = 20 * 2 * PI * 4 / opts::MOTOR_PERIOD;
        runtime.model[0] = model(1.f, decrease, 0.f, 3.f, 1.f);
        MotorPhaseCorrection shifted = spectrum;
        shifted[4].pha -= decrease;
        const CorrectedCurrentLut expected = make_lut(shifted, 0);
        for (int i = 0; i != opts::MOTOR_PERIOD; i++) {
            INFO("idx " << i);
            CHECK(std::abs(runtime.phase_shift(base, i, 2.f) - expected._phase_shift[i]) <= 1);
        }
    }

    SECTION("Size factor scales the harmonic") {
        // size_slope 0.5 per rev/s at 2 rev/s gives factor 1.5.
        runtime.model[0] = model(1.f, 0.f, 0.f, 3.f, 1.f);
        runtime.model[0].size_slope = 0.5f;
        MotorPhaseCorrection scaled = spectrum;
        scaled[4].mag *= 1.5f;
        const CorrectedCurrentLut expected = make_lut(scaled, 0);
        for (int i = 0; i != opts::MOTOR_PERIOD; i++) {
            INFO("idx " << i);
            // The gain also scales the table's own rounding, so up to 2.
            CHECK(std::abs(runtime.phase_shift(base, i, 2.f) - expected._phase_shift[i]) <= 2);
        }
        CHECK_FALSE(runtime.unity_gain);
    }

    SECTION("Two harmonics at offset 0 equal the stock table") {
        speed_model::HarmonicTable table2;
        speed_model::build_harmonic_table(table2, 2, spectrum[2]);
        runtime.harmonic[1] = 2;
        runtime.table[1] = &table2;
        runtime.count = 2;
        runtime.model[0] = model(1.f, 0.f, 0.f, 3.f, 1.f);
        runtime.model[1] = model(1.f, 0.f, 0.f, 3.f, 1.f);
        const CorrectedCurrentLut base2 = make_lut(spectrum, (1u << 4) | (1u << 2));
        for (int i = 0; i != opts::MOTOR_PERIOD; i++) {
            INFO("idx " << i);
            CHECK(std::abs(runtime.phase_shift(base2, i, 1.f) - stock._phase_shift[i]) <= 2);
        }
        CHECK(runtime.unity_gain);
    }

    SECTION("Saturation") {
        MotorPhaseCorrection big {};
        big[2] = { 0.15f, 0.f };
        big[4] = { 0.15f, 0.f };
        speed_model::build_harmonic_table(table, 4, big[4]);
        const CorrectedCurrentLut big_base = make_lut(big, 1u << 4);
        runtime.model[0] = model(1.f, 0.f, 0.f, 3.f, 1.f);
        runtime.last_speed = speed_model::DirectionRuntime::FORCE_RECOMPUTE;
        int max_shift = 0, min_shift = 0;
        for (int i = 0; i != opts::MOTOR_PERIOD; i++) {
            const int shift = runtime.phase_shift(big_base, i, 1.f);
            max_shift = std::max(max_shift, shift);
            min_shift = std::min(min_shift, shift);
        }
        CHECK(max_shift == 127);
        CHECK(min_shift == -127);
    }
}

TEST_CASE("Sweep moves stay within the stock longest move") {
    constexpr float MAX_REVS = 5, SETTLE = 0.2f, ACCEL = 100;
    CHECK(capped_sweep_duration(1.f, 2.f, MAX_REVS, SETTLE, ACCEL) == 2.f);
    const float capped = capped_sweep_duration(3.f, 2.f, MAX_REVS, SETTLE, ACCEL);
    REQUIRE(std::isfinite(capped));
    CHECK(capped < 2.f);
    CHECK(capture_move_revs(3.f, capped, SETTLE, ACCEL) <= MAX_REVS + 1e-4f);
    CHECK(capped * 3.f >= MIN_SWEEP_REVS);
    CHECK(std::isnan(capped_sweep_duration(10.f, 2.f, MAX_REVS, SETTLE, ACCEL)));
}
