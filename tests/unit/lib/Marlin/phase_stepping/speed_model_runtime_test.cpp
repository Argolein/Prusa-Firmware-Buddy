#include <catch2/catch_test_macros.hpp>

#include "test_seams.hpp"

#include <calibration_verify.hpp>
#include <diagnostics.hpp>
#include <heap.h>
#include <phase_stepping.hpp>
#include <speed_model.hpp>
#include <speed_model_runtime.hpp>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

// Counts the frees of the tables, to be balanced against malloc_fallible.
#define free test_free
#include <speed_model_runtime.cpp>
#undef free

using namespace phase_stepping;

namespace {

constexpr uint32_t MODELLED = (1u << 2) | (1u << 4);

CorrectedCurrentLut &lut(int direction) {
    return direction == 0 ? axis_states[X_AXIS].forward_current : axis_states[X_AXIS].backward_current;
}

/// Corrections on harmonics 2, 3 and 4 in both directions; the record keeps
/// all three, with a speed model on 2 and 4.
void prepare() {
    speed_model_runtime::deactivate(X_AXIS);
    test_seams::reset_allocations();
    test_seams::speed_model_enabled = true;
    for (int direction = 0; direction < 2; direction++) {
        lut(direction).modify_correction_table([](auto &table) {
            table[2] = { .mag = 0.05f, .pha = 0.3f };
            table[3] = { .mag = 0.02f, .pha = 1.1f };
            table[4] = { .mag = 0.04f, .pha = -0.7f };
        });
    }
    calibration_verify::begin(X_AXIS);
    const calibration_verify::HarmonicVerification kept { .calibrated = true, .kept = true };
    for (int harmonic : { 2, 3, 4 }) {
        calibration_verify::set(X_AXIS, harmonic, kept, kept);
    }
    const speed_model::HarmonicModel model { .active = true, .v0 = 1.f, .slope = 0.02f, .band_top = 6.f, .max_change = 0.2f };
    calibration_verify::set_speed_model(X_AXIS, 2, model, model);
    calibration_verify::set_speed_model(X_AXIS, 4, model, model);
}

/// Phase shift table of @p direction's corrections without @p excluded.
std::array<int8_t, opts::MOTOR_PERIOD> summed_without(int direction, uint32_t excluded) {
    CorrectedCurrentLut reference = lut(direction);
    reference._speed_model = nullptr;
    reference.set_excluded_harmonics(excluded);
    return reference._phase_shift;
}

void check_stock(int direction) {
    CHECK(lut(direction)._speed_model == nullptr);
    CHECK(lut(direction)._excluded_harmonics == 0);
    CHECK(lut(direction)._phase_shift == summed_without(direction, 0));
}

void check_balanced() {
    CHECK(test_seams::allocations == test_seams::frees);
}

} // namespace

TEST_CASE("speed model runtime builds tables for the active models", "[phase_stepping][runtime]") {
    prepare();
    speed_model_runtime::apply(X_AXIS);

    CHECK(test_seams::allocations == 4);
    CHECK(test_seams::frees == 0);
    for (int direction = 0; direction < 2; direction++) {
        INFO("direction " << direction);
        const CorrectedCurrentLut &used = lut(direction);
        CHECK(used._excluded_harmonics == MODELLED);
        CHECK(used._phase_shift == summed_without(direction, MODELLED));
        REQUIRE(used._speed_model != nullptr);
        const speed_model::DirectionRuntime &runtime = *used._speed_model;
        REQUIRE(runtime.count == 2);
        CHECK(runtime.harmonic[0] == 2);
        CHECK(runtime.harmonic[1] == 4);
        speed_model::HarmonicTable expected;
        speed_model::build_harmonic_table(expected, 4, used.get_correction_table()[4]);
        REQUIRE(runtime.table[1] != nullptr);
        CHECK(*runtime.table[1] == expected);
    }

    speed_model_runtime::deactivate(X_AXIS);
    check_stock(0);
    check_stock(1);
    check_balanced();
}

TEST_CASE("speed model runtime deactivate frees the tables and returns to stock", "[phase_stepping][runtime]") {
    prepare();
    speed_model_runtime::apply(X_AXIS);
    REQUIRE(test_seams::allocations == 4);

    speed_model_runtime::deactivate(X_AXIS);
    CHECK(test_seams::frees == 4);
    check_stock(0);
    check_stock(1);
}

TEST_CASE("speed model runtime stays stock when the model is off", "[phase_stepping][runtime]") {
    prepare();
    test_seams::speed_model_enabled = false;
    speed_model_runtime::apply(X_AXIS);
    CHECK(test_seams::allocation_calls == 0);
    check_stock(0);
    check_stock(1);
}

TEST_CASE("speed model runtime falls back to stock when a table cannot be allocated", "[phase_stepping][runtime]") {
    prepare();
    uint32_t forward_modelled = 0;
    int forward_harmonic = 0;
    SECTION("first table") {
        test_seams::failing_call = 1;
        forward_modelled = 1u << 4;
        forward_harmonic = 4;
    }
    SECTION("second table") {
        test_seams::failing_call = 2;
        forward_modelled = 1u << 2;
        forward_harmonic = 2;
    }
    speed_model_runtime::apply(X_AXIS);

    CHECK(test_seams::allocation_calls == 4);
    CHECK(test_seams::allocations == 3);
    const CorrectedCurrentLut &forward = lut(0);
    CHECK(forward._excluded_harmonics == forward_modelled);
    // The harmonic without a table stays in the summed table.
    CHECK(forward._phase_shift == summed_without(0, forward_modelled));
    REQUIRE(forward._speed_model != nullptr);
    CHECK(forward._speed_model->count == 1);
    CHECK(forward._speed_model->harmonic[0] == forward_harmonic);
    CHECK(forward._speed_model->table[0] != nullptr);
    const CorrectedCurrentLut &backward = lut(1);
    CHECK(backward._excluded_harmonics == MODELLED);
    CHECK(backward._phase_shift == summed_without(1, MODELLED));
    REQUIRE(backward._speed_model != nullptr);
    REQUIRE(backward._speed_model->count == 2);
    CHECK(backward._speed_model->harmonic[0] == 2);
    CHECK(backward._speed_model->harmonic[1] == 4);
    CHECK(backward._speed_model->table[0] != nullptr);
    CHECK(backward._speed_model->table[1] != nullptr);

    speed_model_runtime::deactivate(X_AXIS);
    check_stock(0);
    check_stock(1);
    check_balanced();

    for (int cycle = 0; cycle < 3; cycle++) {
        INFO("cycle " << cycle);
        speed_model_runtime::apply(X_AXIS);
        CHECK(lut(0)._excluded_harmonics == MODELLED);
        CHECK(lut(1)._excluded_harmonics == MODELLED);
        CHECK(test_seams::allocations - test_seams::frees == 4);
        speed_model_runtime::deactivate(X_AXIS);
        check_balanced();
    }
}

TEST_CASE("speed model runtime applied twice keeps one set of tables", "[phase_stepping][runtime]") {
    prepare();
    speed_model_runtime::apply(X_AXIS);
    speed_model_runtime::apply(X_AXIS);
    CHECK(test_seams::allocations == 8);
    CHECK(test_seams::allocations - test_seams::frees == 4);
    CHECK(lut(0)._excluded_harmonics == MODELLED);

    speed_model_runtime::deactivate(X_AXIS);
    check_stock(0);
    check_stock(1);
    check_balanced();
}
