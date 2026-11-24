#include <catch2/catch_test_macros.hpp>
#include <lut.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace phase_stepping;

namespace {

// Phase shift the table should hold, in 1/SIN_PERIOD of an electrical period,
// saturated at the int8 range.
int expected_shift(const MotorPhaseCorrection &spectrum, size_t idx) {
    const float theta = 2 * std::numbers::pi_v<float> * idx / opts::MOTOR_PERIOD;
    float shift = 0;
    for (size_t n = 0; n != spectrum.size(); n++) {
        shift += spectrum[n].mag * std::sin(n * theta + spectrum[n].pha);
    }
    const long rounded = std::lround(shift * opts::SIN_PERIOD / (2 * std::numbers::pi_v<float>));
    return static_cast<int>(std::clamp(rounded, -127L, 127L));
}

void check_saturates(const MotorPhaseCorrection &spectrum) {
    CorrectedCurrentLut lut;
    lut.modify_correction_table([&](MotorPhaseCorrection &table) {
        table = spectrum;
    });

    const auto [min_it, max_it] = std::minmax_element(lut._phase_shift.begin(), lut._phase_shift.end());
    CHECK(*max_it == 127);
    CHECK(*min_it == -127);

    for (size_t i = 0; i != lut._phase_shift.size(); i++) {
        INFO("idx " << i);
        // Rounding of the fixed-point sum may differ by one from the float reference.
        CHECK(std::abs(lut._phase_shift[i] - expected_shift(spectrum, i)) <= 1);
    }
}

} // namespace

TEST_CASE("Summed phase shift saturates instead of wrapping") {
    SECTION("Single harmonic above the int8 range") {
        // 0.3 rad is about 196 / 4096 of a period, beyond the int8 table.
        MotorPhaseCorrection spectrum {};
        spectrum[2] = { 0.3f, 0.f };
        check_saturates(spectrum);
    }

    SECTION("Sum that overflows a 32-bit accumulator") {
        // Each term stays below the int32 limit; their in-phase sum at
        // idx = MOTOR_PERIOD / 4 does not.
        MotorPhaseCorrection spectrum {};
        spectrum[1] = { 0.3f, 0.f };
        spectrum[3] = { 0.3f, -std::numbers::pi_v<float> };
        check_saturates(spectrum);
    }
}
