#include "phase_stepping_mock.hpp"

#include <phase_stepping_renderer.h>

#include <feature/phase_stepping/phase_stepping.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <string_view>

using json::JsonResult;
using nhttp::link_content::PhaseSteppingRenderer;
using std::string;
using std::string_view;

namespace {

namespace verify = phase_stepping::calibration_verify;

/// A correction array of all harmonics, zero except @p harmonic, which renders as @p pair.
string spectrum(int harmonic = -1, const char *pair = "") {
    string out = "[";
    for (int i = 0; i <= phase_stepping::opts::CORRECTION_HARMONICS; i++) {
        if (i != 0) {
            out += ",";
        }
        out += i == harmonic ? pair : "[0,0]";
    }
    return out + "]";
}

void populate() {
    phase_stepping_mock::speed_model_enabled = true;
    phase_stepping_mock::free_heap = 12345;
    phase_stepping_mock::isr = { .max_cycles = 900, .mean_cycles = 400, .samples = 1000 };
    phase_stepping_mock::heap = { .valid = true, .min_free_bytes = 2048, .free_chunks_at_min = 7 };
    phase_stepping_mock::missed_tx = { 3, 0 };

    auto &x = phase_stepping::axis_states[0];
    x.enabled = true;
    x.forward_current._spectrum[2] = { 0.5f, 0.25f };
    x.backward_current._spectrum[4] = { 0.25f, 1.5f };

    verify::MotorVerification &rec_x = phase_stepping_mock::records[0];
    rec_x.version = 2;
    verify::HarmonicVerification &fwd2 = rec_x.direction[0][2];
    fwd2.calibrated = true;
    fwd2.kept = true;
    fwd2.ratio = { 0.5f, 0.25f };
    fwd2.speed_model = { .active = true, .v0 = 1.5f, .slope = 0.25f, .slope_se = 0.125f, .size_slope = 0.5f, .band_top = 4.0f, .max_change = 0.75f };
    verify::HarmonicVerification &fwd4 = rec_x.direction[0][4];
    fwd4.calibrated = true;
    fwd4.ratio = { NAN, 1.5f };
    fwd4.reason = verify::Reason::ratio_too_high;
    rec_x.direction[1][4].speed_model = { .active = false, .v0 = 2.0f, .max_change = 0.5f };
    phase_stepping_mock::records[1].version = 1;

    auto &check = phase_stepping_mock::check;
    check.valid = true;
    check.suggest_recalibration = true;
    check.motor[0].direction[0][2] = { .checked = true, .ratio = 2.5f };
    check.motor[0].direction[0][4] = { .checked = true, .ratio = NAN };
    check.motor[1].direction[1][3] = { .checked = true, .ratio = 0.5f };
}

const string EMPTY_MOTOR_TAIL
    = ",\"forward\":" + spectrum() + ",\"backward\":" + spectrum()
    + ",\"verification\":{\"forward\":[],\"backward\":[]}"
      ",\"speed_model\":{\"forward\":[],\"backward\":[]}}";

const string EXPECTED_EMPTY
    = "{\"speed_model_enabled\":false,\"calibration_version\":0,\"free_heap_bytes\":0,"
      "\"isr\":{\"max_cycles\":0,\"mean_cycles\":0,\"samples\":0,\"cpu_hz\":180000000},"
      "\"calibration_heap\":{\"valid\":false,\"min_free_bytes\":0,\"free_chunks_at_min\":0},"
      "\"motors\":["
      "{\"axis\":\"x\",\"motor\":\"A\",\"enabled\":false,\"missed_tx\":0"
    + EMPTY_MOTOR_TAIL
    + ",{\"axis\":\"y\",\"motor\":\"B\",\"enabled\":false,\"missed_tx\":0"
    + EMPTY_MOTOR_TAIL
    + "],\"check\":{\"valid\":false,\"suggest_recalibration\":false,\"motors\":["
      "{\"axis\":\"x\",\"forward\":[],\"backward\":[]},"
      "{\"axis\":\"y\",\"forward\":[],\"backward\":[]}]}}";

const string EXPECTED_POPULATED
    = "{\"speed_model_enabled\":true,\"calibration_version\":2,\"free_heap_bytes\":12345,"
      "\"isr\":{\"max_cycles\":900,\"mean_cycles\":400,\"samples\":1000,\"cpu_hz\":180000000},"
      "\"calibration_heap\":{\"valid\":true,\"min_free_bytes\":2048,\"free_chunks_at_min\":7},"
      "\"motors\":["
      "{\"axis\":\"x\",\"motor\":\"A\",\"enabled\":true,\"missed_tx\":3"
      ",\"forward\":"
    + spectrum(2, "[0.5,0.25]")
    + ",\"backward\":"
    + spectrum(4, "[0.25,1.5]")
    + ",\"verification\":{\"forward\":["
      "{\"harmonic\":2,\"kept\":true,\"ratio\":[0.5,0.25],\"reason\":0},"
      "{\"harmonic\":4,\"kept\":false,\"ratio\":[null,1.5],\"reason\":1}"
      "],\"backward\":[]}"
      ",\"speed_model\":{\"forward\":["
      "{\"harmonic\":2,\"active\":true,\"v0\":1.5,\"slope\":0.25,\"slope_se\":0.125,\"size_slope\":0.5,\"band_top\":4,\"max_change\":0.75}"
      "],\"backward\":["
      "{\"harmonic\":4,\"active\":false,\"v0\":2,\"slope\":0,\"slope_se\":0,\"size_slope\":0,\"band_top\":0,\"max_change\":0.5}"
      "]}}"
      ",{\"axis\":\"y\",\"motor\":\"B\",\"enabled\":false,\"missed_tx\":0"
    + EMPTY_MOTOR_TAIL
    + "],\"check\":{\"valid\":true,\"suggest_recalibration\":true,\"motors\":["
      "{\"axis\":\"x\",\"forward\":[{\"harmonic\":2,\"ratio\":2.5},{\"harmonic\":4,\"ratio\":null}],\"backward\":[]},"
      "{\"axis\":\"y\",\"forward\":[],\"backward\":[{\"harmonic\":3,\"ratio\":0.5}]}]}}";

string render_whole() {
    PhaseSteppingRenderer renderer;
    constexpr size_t BUF_SIZE = 8192;
    uint8_t buffer[BUF_SIZE];
    const auto [result, written] = renderer.render(buffer, BUF_SIZE);
    REQUIRE(result == JsonResult::Complete);
    return string(reinterpret_cast<char *>(buffer), written);
}

} // namespace

TEST_CASE("Phase stepping renderer - nothing calibrated") {
    phase_stepping_mock::reset();
    REQUIRE(render_whole() == EXPECTED_EMPTY);
}

TEST_CASE("Phase stepping renderer - populated record in one buffer") {
    phase_stepping_mock::reset();
    populate();
    REQUIRE(render_whole() == EXPECTED_POPULATED);
}

TEST_CASE("Phase stepping renderer - populated record split across buffers") {
    phase_stepping_mock::reset();
    populate();
    PhaseSteppingRenderer renderer;

    // A speed model entry is emitted whole and is over 100 characters.
    size_t increment = 160;
    SECTION("small") { increment = 160; }
    SECTION("mid") { increment = 256; }
    SECTION("large") { increment = 512; }

    string response;
    size_t segments = 0;
    auto result = JsonResult::Incomplete;

    while (result != JsonResult::Complete) {
        uint8_t buffer[increment];
        const auto [result_partial, written] = renderer.render(buffer, increment);
        REQUIRE(written <= increment);
        REQUIRE(written > 0);
        response += string_view(reinterpret_cast<char *>(buffer), written);
        result = result_partial;
        segments++;
    }

    REQUIRE(segments > 1);
    REQUIRE(response == EXPECTED_POPULATED);
}

TEST_CASE("Phase stepping renderer - invalid check hides its entries") {
    phase_stepping_mock::reset();
    populate();
    phase_stepping_mock::check.valid = false;

    const size_t check_at = EXPECTED_POPULATED.find(",\"check\":");
    REQUIRE(check_at != string::npos);
    const string expected = EXPECTED_POPULATED.substr(0, check_at)
        + ",\"check\":{\"valid\":false,\"suggest_recalibration\":false,\"motors\":["
          "{\"axis\":\"x\",\"forward\":[],\"backward\":[]},"
          "{\"axis\":\"y\",\"forward\":[],\"backward\":[]}]}}";
    REQUIRE(render_whole() == expected);
}
