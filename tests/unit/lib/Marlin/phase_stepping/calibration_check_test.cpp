#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <calibration_check.hpp>

#include <cmath>

using namespace phase_stepping::calibration_check;
using Catch::Matchers::WithinAbs;

TEST_CASE("check ratio is residual over baseline", "[phase_stepping][check]") {
    REQUIRE_THAT(residual_ratio(0.03f, 0.01f), WithinAbs(3.0, 1e-6));
    REQUIRE_THAT(residual_ratio(0.01f, 0.02f), WithinAbs(0.5, 1e-6));
}

TEST_CASE("check ratio without a baseline is NaN", "[phase_stepping][check]") {
    CHECK(std::isnan(residual_ratio(0.01f, 0.f)));
    CHECK(std::isnan(residual_ratio(0.01f, -1.f)));
    CHECK(std::isnan(residual_ratio(0.01f, NAN)));
    CHECK(std::isnan(residual_ratio(0.01f, INFINITY)));
    CHECK(std::isnan(residual_ratio(NAN, 0.01f)));
}

TEST_CASE("check suggests recalibration above the threshold", "[phase_stepping][check]") {
    CheckResult result;
    result.motor[0].direction[0][2] = { .checked = true, .ratio = 1.1f };
    result.motor[1].direction[1][4] = { .checked = true, .ratio = RECALIBRATION_RATIO };

    conclude(result, true);
    CHECK(result.valid);
    CHECK_FALSE(result.suggest_recalibration);

    result.motor[1].direction[0][4] = { .checked = true, .ratio = 2.01f };
    conclude(result, true);
    CHECK(result.valid);
    CHECK(result.suggest_recalibration);
}

TEST_CASE("check without any baseline or with a failed move is not valid", "[phase_stepping][check]") {
    CheckResult result;
    conclude(result, true);
    CHECK_FALSE(result.valid);
    CHECK_FALSE(result.suggest_recalibration);

    // Checked, but no baseline: NaN ratio never suggests recalibration.
    result.motor[0].direction[0][2] = { .checked = true, .ratio = NAN };
    conclude(result, true);
    CHECK_FALSE(result.valid);
    CHECK_FALSE(result.suggest_recalibration);

    result.motor[0].direction[1][2] = { .checked = true, .ratio = 5.f };
    conclude(result, false);
    CHECK_FALSE(result.valid);
    CHECK_FALSE(result.suggest_recalibration);
}

TEST_CASE("last check is kept", "[phase_stepping][check]") {
    CHECK_FALSE(last().valid);
    CheckResult result;
    result.motor[1].direction[0][2] = { .checked = true, .ratio = 3.f };
    conclude(result, true);
    set_last(result);
    CHECK(last().valid);
    CHECK(last().suggest_recalibration);
    REQUIRE_THAT(last().motor[1].direction[0][2].ratio, WithinAbs(3.0, 1e-6));
}
