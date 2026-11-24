#include "calibration_verify.hpp"

#include <bsod/bsod.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unistd.h>

namespace phase_stepping::calibration_verify {

namespace {

    constexpr size_t MOTOR_COUNT = 2;
    constexpr size_t DIRECTION_COUNT = 2;
    constexpr size_t HARMONIC_COUNT = opts::CORRECTION_HARMONICS + 1;

    /// Magic of `/system/phase_step_{x,y}_speed`, "PSSP" in little endian.
    constexpr uint32_t FILE_MAGIC = 0x50535350;

    /// Fixed file header. Later versions keep the first four fields and use the
    /// reserved bytes (e.g. for the calibration heap minimum).
    struct FileHeader {
        uint32_t magic;
        uint16_t version;
        uint16_t header_size;
        uint16_t record_size;
        uint16_t record_count;
        // Version 2 from here on, zero in version 1.
        uint32_t heap_min_free_bytes;
        uint32_t heap_free_chunks_at_min;
        uint8_t heap_valid;
        uint8_t reserved[11];
    };
    static_assert(sizeof(FileHeader) == 32);

    constexpr uint8_t FLAG_CALIBRATED = 1 << 0;
    constexpr uint8_t FLAG_KEPT = 1 << 1;
    constexpr uint8_t FLAG_SPEED_MODEL_ACTIVE = 1 << 2;

    /// One record per direction and harmonic, forward harmonics 0..16 first.
    /// The speed model fields are zero in version 1, the baseline fields are
    /// zero in records written before the check existed.
    struct FileRecord {
        uint8_t harmonic;
        uint8_t flags;
        uint8_t reason;
        uint8_t reserved0;
        float ratio[VERIFY_RUNS];
        float v0;
        float slope;
        float slope_se;
        float size_slope;
        float band_top;
        float max_change;
        float baseline;
        float baseline_speed;
        uint8_t reserved[4];
    };
    static_assert(sizeof(FileRecord) == 48);

    constexpr uint16_t RECORD_COUNT = DIRECTION_COUNT * HARMONIC_COUNT;

    std::array<MotorVerification, MOTOR_COUNT> records;

    size_t motor_index(AxisEnum axis) {
        switch (axis) {
        case AxisEnum::X_AXIS:
            return 0;
        case AxisEnum::Y_AXIS:
            return 1;
        default:
            bsod("Unsupported axis");
        }
    }

    const char *file_path(AxisEnum axis) {
        return motor_index(axis) == 0 ? "/system/phase_step_x_speed" : "/system/phase_step_y_speed";
    }

    bool header_matches(const FileHeader &header) {
        return header.magic == FILE_MAGIC
            && header.version >= MIN_FORMAT_VERSION && header.version <= FORMAT_VERSION
            && header.header_size == sizeof(FileHeader)
            && header.record_size == sizeof(FileRecord)
            && header.record_count == RECORD_COUNT;
    }

    bool load_from(FILE *file, MotorVerification &record) {
        FileHeader header {};
        if (fread(&header, 1, sizeof(header), file) != sizeof(header) || !header_matches(header)) {
            return false;
        }
        for (auto &direction : record.direction) {
            for (size_t harmonic = 0; harmonic != direction.size(); harmonic++) {
                FileRecord stored {};
                if (fread(&stored, 1, sizeof(stored), file) != sizeof(stored) || stored.harmonic != harmonic) {
                    return false;
                }
                HarmonicVerification &item = direction[harmonic];
                std::copy(std::begin(stored.ratio), std::end(stored.ratio), item.ratio.begin());
                item.calibrated = stored.flags & FLAG_CALIBRATED;
                item.kept = stored.flags & FLAG_KEPT;
                item.reason = static_cast<Reason>(stored.reason);
                item.baseline = stored.baseline;
                item.baseline_speed = stored.baseline_speed;
                if (header.version >= 2) {
                    item.speed_model = {
                        .active = (stored.flags & FLAG_SPEED_MODEL_ACTIVE) != 0,
                        .v0 = stored.v0,
                        .slope = stored.slope,
                        .slope_se = stored.slope_se,
                        .size_slope = stored.size_slope,
                        .band_top = stored.band_top,
                        .max_change = stored.max_change,
                    };
                }
            }
        }
        if (header.version >= 2) {
            record.heap = {
                .valid = header.heap_valid != 0,
                .min_free_bytes = header.heap_min_free_bytes,
                .free_chunks_at_min = header.heap_free_chunks_at_min,
            };
        }
        record.version = header.version;
        return true;
    }

} // namespace

HarmonicVerification judge(float magnitude, const std::array<float, VERIFY_RUNS> &ratio) {
    HarmonicVerification result { .ratio = ratio, .calibrated = true };
    if (magnitude < MIN_CORRECTION_RAD) {
        result.reason = Reason::below_min_size;
    } else if (std::all_of(ratio.begin(), ratio.end(), [](float r) { return std::isfinite(r) && r < MAX_KEPT_RATIO; })) {
        result.kept = true;
        result.reason = Reason::kept;
    } else {
        result.reason = Reason::ratio_too_high;
    }
    return result;
}

const MotorVerification &record(AxisEnum axis) {
    return records[motor_index(axis)];
}

void begin(AxisEnum axis) {
    records[motor_index(axis)] = { .version = FORMAT_VERSION };
}

void set(AxisEnum axis, int harmonic, const HarmonicVerification &forward, const HarmonicVerification &backward) {
    MotorVerification &record = records[motor_index(axis)];
    record.direction[0][harmonic] = forward;
    record.direction[1][harmonic] = backward;
}

void set_speed_model(AxisEnum axis, int harmonic, const speed_model::HarmonicModel &forward, const speed_model::HarmonicModel &backward) {
    MotorVerification &record = records[motor_index(axis)];
    record.direction[0][harmonic].speed_model = forward;
    record.direction[1][harmonic].speed_model = backward;
}

void drop_speed_model(AxisEnum axis, int direction, int harmonic) {
    MotorVerification &record = records[motor_index(axis)];
    HarmonicVerification &entry = record.direction[direction][harmonic];
    if (record.version == 0 || (!entry.speed_model.active && std::isnan(entry.baseline))) {
        return;
    }
    // A hand-written value invalidates both the model and the check's reference.
    entry.speed_model.active = false;
    entry.baseline = NAN;
    entry.baseline_speed = NAN;
    save(axis);
}

void save(AxisEnum axis) {
    MotorVerification &record = records[motor_index(axis)];
    if (record.version == 0) {
        return;
    }
    if (const diagnostics::CalibrationHeap heap = diagnostics::last_calibration_heap(); heap.valid) {
        record.heap = heap;
    }
    FILE *file = fopen(file_path(axis), "wb");
    if (!file) {
        return;
    }
    const FileHeader header {
        .magic = FILE_MAGIC,
        .version = FORMAT_VERSION,
        .header_size = sizeof(FileHeader),
        .record_size = sizeof(FileRecord),
        .record_count = RECORD_COUNT,
        .heap_min_free_bytes = record.heap.min_free_bytes,
        .heap_free_chunks_at_min = record.heap.free_chunks_at_min,
        .heap_valid = record.heap.valid,
        .reserved = {},
    };
    fwrite(&header, 1, sizeof(header), file);
    for (const auto &direction : record.direction) {
        for (size_t harmonic = 0; harmonic != direction.size(); harmonic++) {
            const HarmonicVerification &item = direction[harmonic];
            const speed_model::HarmonicModel &model = item.speed_model;
            FileRecord stored {
                .harmonic = static_cast<uint8_t>(harmonic),
                .flags = static_cast<uint8_t>((item.calibrated ? FLAG_CALIBRATED : 0) | (item.kept ? FLAG_KEPT : 0)
                    | (model.active ? FLAG_SPEED_MODEL_ACTIVE : 0)),
                .reason = static_cast<uint8_t>(item.reason),
                .reserved0 = 0,
                .ratio = {},
                .v0 = model.v0,
                .slope = model.slope,
                .slope_se = model.slope_se,
                .size_slope = model.size_slope,
                .band_top = model.band_top,
                .max_change = model.max_change,
                .baseline = item.baseline,
                .baseline_speed = item.baseline_speed,
                .reserved = {},
            };
            std::copy(item.ratio.begin(), item.ratio.end(), std::begin(stored.ratio));
            fwrite(&stored, 1, sizeof(stored), file);
        }
    }
    fclose(file);
}

void load(AxisEnum axis) {
    MotorVerification &record = records[motor_index(axis)];
    record = {};
    FILE *file = fopen(file_path(axis), "rb");
    if (!file) {
        return;
    }
    if (!load_from(file, record)) {
        record = {};
    }
    fclose(file);
    if (record.heap.valid && !diagnostics::last_calibration_heap().valid) {
        diagnostics::restore_calibration_heap(record.heap);
    }
}

void remove(AxisEnum axis) {
    records[motor_index(axis)] = {};
    unlink(file_path(axis)); // a missing file is the normal case
}

} // namespace phase_stepping::calibration_verify
