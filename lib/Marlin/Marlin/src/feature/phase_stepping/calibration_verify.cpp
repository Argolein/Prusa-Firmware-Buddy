#include "calibration_verify.hpp"

#include <bsod/bsod.h>

#include <algorithm>
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
        uint8_t reserved[20];
    };
    static_assert(sizeof(FileHeader) == 32);

    constexpr uint8_t FLAG_CALIBRATED = 1 << 0;
    constexpr uint8_t FLAG_KEPT = 1 << 1;

    /// One record per direction and harmonic, forward harmonics 0..16 first.
    /// The reserved bytes hold the speed model fields of later versions.
    struct FileRecord {
        uint8_t harmonic;
        uint8_t flags;
        uint8_t reason;
        uint8_t reserved0;
        float ratio[VERIFY_RUNS];
        uint8_t reserved[36];
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
            && header.version == FORMAT_VERSION
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
            }
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

void save(AxisEnum axis) {
    const MotorVerification &record = records[motor_index(axis)];
    if (record.version == 0) {
        return;
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
        .reserved = {},
    };
    fwrite(&header, 1, sizeof(header), file);
    for (const auto &direction : record.direction) {
        for (size_t harmonic = 0; harmonic != direction.size(); harmonic++) {
            const HarmonicVerification &item = direction[harmonic];
            FileRecord stored {
                .harmonic = static_cast<uint8_t>(harmonic),
                .flags = static_cast<uint8_t>((item.calibrated ? FLAG_CALIBRATED : 0) | (item.kept ? FLAG_KEPT : 0)),
                .reason = static_cast<uint8_t>(item.reason),
                .reserved0 = 0,
                .ratio = {},
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
}

void remove(AxisEnum axis) {
    records[motor_index(axis)] = {};
    unlink(file_path(axis)); // a missing file is the normal case
}

} // namespace phase_stepping::calibration_verify
