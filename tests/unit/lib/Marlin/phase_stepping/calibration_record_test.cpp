#include <catch2/catch_test_macros.hpp>

#include "test_seams.hpp"

#include <calibration_verify.hpp>

#include <bsod/bsod.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

// The firmware hard-codes its paths under `/system`; the seam maps them into a
// host temp directory.
#define fopen  test_fopen
#define unlink test_unlink
#include <calibration_verify.cpp>
#undef fopen
#undef unlink

using namespace phase_stepping;
using namespace phase_stepping::calibration_verify;

namespace {

constexpr size_t HEADER_SIZE = 32;
constexpr size_t RECORD_SIZE = 48;
constexpr size_t HARMONICS = opts::CORRECTION_HARMONICS + 1;
constexpr size_t FILE_SIZE = HEADER_SIZE + 2 * HARMONICS * RECORD_SIZE;

std::string x_file() {
    return test_seams::host_path("/system/phase_step_x_speed");
}

void write_file(const std::vector<uint8_t> &bytes) {
    FILE *file = std::fopen(x_file().c_str(), "wb");
    REQUIRE(file);
    REQUIRE(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    std::fclose(file);
}

std::vector<uint8_t> read_file() {
    std::vector<uint8_t> bytes;
    FILE *file = std::fopen(x_file().c_str(), "rb");
    REQUIRE(file);
    for (int c; (c = std::fgetc(file)) != EOF;) {
        bytes.push_back(static_cast<uint8_t>(c));
    }
    std::fclose(file);
    return bytes;
}

template <typename T>
void put(std::vector<uint8_t> &bytes, size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

/// File bytes in the version 1 layout, written field by field: header
/// (magic, version, header size, record size, record count), then forward
/// and backward records of harmonics 0..16 (harmonic, flags, reason, ratios,
/// zero model fields, baseline, baseline speed). Forward harmonic 3 is kept,
/// backward harmonic 3 was rejected, the rest is uncalibrated.
std::vector<uint8_t> v1_bytes(uint16_t version = 1, uint32_t magic = 0x50535350) {
    std::vector<uint8_t> bytes(FILE_SIZE, 0);
    put<uint32_t>(bytes, 0, magic);
    put<uint16_t>(bytes, 4, version);
    put<uint16_t>(bytes, 6, HEADER_SIZE);
    put<uint16_t>(bytes, 8, RECORD_SIZE);
    put<uint16_t>(bytes, 10, 2 * HARMONICS);
    // Heap fields only exist from version 2 on.
    put<uint32_t>(bytes, 12, 4000);
    put<uint32_t>(bytes, 16, 5);
    put<uint8_t>(bytes, 20, 1);
    for (size_t i = 0; i != 2 * HARMONICS; i++) {
        const size_t at = HEADER_SIZE + i * RECORD_SIZE;
        put<uint8_t>(bytes, at, i % HARMONICS);
        put<float>(bytes, at + 4, NAN);
        put<float>(bytes, at + 8, NAN);
        put<float>(bytes, at + 36, NAN);
        put<float>(bytes, at + 40, NAN);
    }
    const size_t fwd3 = HEADER_SIZE + 3 * RECORD_SIZE;
    put<uint8_t>(bytes, fwd3 + 1, 0b111); // calibrated, kept, and a model flag v1 does not know
    put<uint8_t>(bytes, fwd3 + 2, 0);
    put<float>(bytes, fwd3 + 4, 0.25f);
    put<float>(bytes, fwd3 + 8, 0.5f);
    put<float>(bytes, fwd3 + 12, 1.5f); // v0, ignored in version 1
    put<float>(bytes, fwd3 + 36, 0.02f);
    put<float>(bytes, fwd3 + 40, 3.f);
    const size_t bwd3 = HEADER_SIZE + (HARMONICS + 3) * RECORD_SIZE;
    put<uint8_t>(bytes, bwd3 + 1, 0b001);
    put<uint8_t>(bytes, bwd3 + 2, 1);
    put<float>(bytes, bwd3 + 4, 0.9f);
    put<float>(bytes, bwd3 + 8, 1.2f);
    return bytes;
}

bool same(float a, float b) {
    return (std::isnan(a) && std::isnan(b)) || a == b;
}

void check_same(const HarmonicVerification &actual, const HarmonicVerification &expected) {
    CHECK(same(actual.ratio[0], expected.ratio[0]));
    CHECK(same(actual.ratio[1], expected.ratio[1]));
    CHECK(actual.calibrated == expected.calibrated);
    CHECK(actual.kept == expected.kept);
    CHECK(actual.reason == expected.reason);
    CHECK(same(actual.baseline, expected.baseline));
    CHECK(same(actual.baseline_speed, expected.baseline_speed));
    const speed_model::HarmonicModel &a = actual.speed_model, &e = expected.speed_model;
    CHECK(a.active == e.active);
    CHECK(a.v0 == e.v0);
    CHECK(a.slope == e.slope);
    CHECK(a.slope_se == e.slope_se);
    CHECK(a.size_slope == e.size_slope);
    CHECK(a.band_top == e.band_top);
    CHECK(a.max_change == e.max_change);
}

void check_empty(const MotorVerification &record) {
    CHECK(record.version == 0);
    CHECK_FALSE(record.heap.valid);
    for (const auto &direction : record.direction) {
        for (const auto &item : direction) {
            check_same(item, HarmonicVerification {});
        }
    }
}

const HarmonicVerification FORWARD_2 {
    .ratio = { 0.3f, 0.4f },
    .calibrated = true,
    .kept = true,
    .reason = Reason::kept,
    .baseline = 0.01f,
    .baseline_speed = 2.5f,
    .speed_model = {},
};
const HarmonicVerification BACKWARD_2 {
    .ratio = { 0.8f, NAN },
    .calibrated = true,
    .kept = false,
    .reason = Reason::ratio_too_high,
};
const HarmonicVerification FORWARD_4 {
    .ratio = { NAN, NAN },
    .calibrated = true,
    .kept = false,
    .reason = Reason::below_min_size,
};
const HarmonicVerification BACKWARD_4 {
    .ratio = { 0.1f, 0.2f },
    .calibrated = true,
    .kept = true,
    .reason = Reason::kept,
    .baseline = 0.03f,
    .baseline_speed = 4.f,
};
const speed_model::HarmonicModel MODEL_FORWARD_2 {
    .active = true, .v0 = 1.f, .slope = 0.05f, .slope_se = 0.004f, .size_slope = -0.01f, .band_top = 6.f, .max_change = 0.2f
};
const speed_model::HarmonicModel MODEL_BACKWARD_4 {
    .active = false, .v0 = 2.f, .slope = -0.03f, .slope_se = 0.002f, .size_slope = 0.02f, .band_top = 5.f, .max_change = 0.1f
};

/// A calibrated record of X in RAM, not saved.
void fill_record() {
    begin(X_AXIS);
    set(X_AXIS, 2, FORWARD_2, BACKWARD_2);
    set(X_AXIS, 4, FORWARD_4, BACKWARD_4);
    set_speed_model(X_AXIS, 2, MODEL_FORWARD_2, {});
    set_speed_model(X_AXIS, 4, {}, MODEL_BACKWARD_4);
}

void reset() {
    remove(X_AXIS);
    test_seams::calibration_heap = {};
}

} // namespace

TEST_CASE("record version 2 survives save and load", "[phase_stepping][record]") {
    reset();
    fill_record();
    test_seams::calibration_heap = { .valid = true, .min_free_bytes = 12345, .free_chunks_at_min = 7 };
    save(X_AXIS);

    const std::vector<uint8_t> bytes = read_file();
    REQUIRE(bytes.size() == FILE_SIZE);
    uint16_t version;
    std::memcpy(&version, bytes.data() + 4, sizeof(version));
    CHECK(version == 2);

    test_seams::calibration_heap = {};
    load(X_AXIS);
    const MotorVerification &loaded = record(X_AXIS);
    CHECK(loaded.version == 2);
    CHECK(loaded.heap.valid);
    CHECK(loaded.heap.min_free_bytes == 12345);
    CHECK(loaded.heap.free_chunks_at_min == 7);
    CHECK(test_seams::calibration_heap.valid);
    CHECK(test_seams::calibration_heap.min_free_bytes == 12345);

    HarmonicVerification forward_2 = FORWARD_2, backward_4 = BACKWARD_4, backward_2 = BACKWARD_2, forward_4 = FORWARD_4;
    forward_2.speed_model = MODEL_FORWARD_2;
    backward_4.speed_model = MODEL_BACKWARD_4;
    for (size_t harmonic = 0; harmonic != HARMONICS; harmonic++) {
        INFO("harmonic " << harmonic);
        check_same(loaded.direction[0][harmonic], harmonic == 2 ? forward_2 : harmonic == 4 ? forward_4
                                                                                            : HarmonicVerification {});
        check_same(loaded.direction[1][harmonic], harmonic == 2 ? backward_2 : harmonic == 4 ? backward_4
                                                                                             : HarmonicVerification {});
    }
}

TEST_CASE("record version 1 loads without speed models", "[phase_stepping][record]") {
    reset();
    write_file(v1_bytes());
    load(X_AXIS);
    const MotorVerification &loaded = record(X_AXIS);
    CHECK(loaded.version == 1);
    CHECK_FALSE(loaded.heap.valid);
    CHECK_FALSE(test_seams::calibration_heap.valid);

    const HarmonicVerification forward_3 {
        .ratio = { 0.25f, 0.5f }, .calibrated = true, .kept = true, .reason = Reason::kept, .baseline = 0.02f, .baseline_speed = 3.f
    };
    const HarmonicVerification backward_3 {
        .ratio = { 0.9f, 1.2f }, .calibrated = true, .kept = false, .reason = Reason::ratio_too_high
    };
    for (size_t harmonic = 0; harmonic != HARMONICS; harmonic++) {
        INFO("harmonic " << harmonic);
        check_same(loaded.direction[0][harmonic], harmonic == 3 ? forward_3 : HarmonicVerification {});
        check_same(loaded.direction[1][harmonic], harmonic == 3 ? backward_3 : HarmonicVerification {});
    }
}

TEST_CASE("unreadable record files load as an empty record", "[phase_stepping][record]") {
    reset();
    fill_record();
    save(X_AXIS);
    const std::vector<uint8_t> saved = read_file();
    REQUIRE(saved.size() == FILE_SIZE);
    // A loaded record must not keep anything from before the load, the heap
    // minimum included.
    fill_record();
    test_seams::calibration_heap = { .valid = true, .min_free_bytes = 12345, .free_chunks_at_min = 7 };
    save(X_AXIS);
    test_seams::calibration_heap = {};
    REQUIRE(record(X_AXIS).heap.valid);
    std::vector<uint8_t> broken = saved;

    SECTION("short file, inside the header") {
        write_file({ saved.begin(), saved.begin() + 20 });
    }
    SECTION("short file, after some complete records") {
        write_file({ saved.begin(), saved.begin() + HEADER_SIZE + 20 * RECORD_SIZE + 10 });
    }
    SECTION("wrong magic") {
        write_file(v1_bytes(1, 0x50535351));
    }
    SECTION("garbage") {
        write_file(std::vector<uint8_t>(FILE_SIZE, 0xa5));
    }
    SECTION("wrong version") {
        write_file(v1_bytes(3));
    }
    SECTION("version 0") {
        write_file(v1_bytes(0));
    }
    SECTION("wrong header size") {
        put<uint16_t>(broken, 6, HEADER_SIZE + 4);
        write_file(broken);
    }
    SECTION("wrong record size") {
        put<uint16_t>(broken, 8, RECORD_SIZE - 4);
        write_file(broken);
    }
    SECTION("wrong record count") {
        put<uint16_t>(broken, 10, 2 * HARMONICS - 1);
        write_file(broken);
    }
    SECTION("record out of harmonic order") {
        // The 20th record is backward harmonic 2.
        put<uint8_t>(broken, HEADER_SIZE + 19 * RECORD_SIZE, 3);
        write_file(broken);
    }
    SECTION("missing file") {
        REQUIRE(std::remove(x_file().c_str()) == 0);
    }

    load(X_AXIS);
    check_empty(record(X_AXIS));
    CHECK_FALSE(test_seams::calibration_heap.valid);
}
