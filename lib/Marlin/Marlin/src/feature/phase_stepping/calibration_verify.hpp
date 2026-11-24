#pragma once

#include "common.hpp"
#include "diagnostics.hpp"
#include "speed_model.hpp"

#include <core/types.h>

#include <array>
#include <cmath>
#include <cstdint>

/// Verification of the phase stepping calibration (INDX only).
///
/// After the stock tuning of a harmonic, the calibration measures the residual
/// vibration at that harmonic with the new correction and with the harmonic
/// zeroed, twice per direction. The correction is kept only if every ratio
/// residual(new) / residual(zero) is below `MAX_KEPT_RATIO`; otherwise zero is
/// stored. The results are kept in RAM per motor and persisted in
/// `/system/phase_step_{x,y}_speed`, next to the stock correction files,
/// together with the speed model of each harmonic (version 2) and the heap
/// minimum of the calibration.
namespace phase_stepping::calibration_verify {

/// Provisional threshold, to be retuned from the recorded spreads.
inline constexpr float MAX_KEPT_RATIO = 0.7f;
/// Smaller corrections are not realized cleanly by the int8 phase-shift table.
inline constexpr float MIN_CORRECTION_RAD = 0.002f;
/// Upper bound of the magnitude search. From π/8 rad up, the fixed-point
/// product in `CorrectedCurrentLut::_phase_shift_for_harmonic` overflows int32.
inline constexpr float MAX_SEARCH_MAGNITUDE = 0.39f;
/// Each pair of moves (new correction, harmonic zeroed) runs this many times.
inline constexpr int VERIFY_RUNS = 2;

/// Version of the record format written by this firmware.
inline constexpr uint16_t FORMAT_VERSION = 2;
/// Oldest record format this firmware still reads (without speed models).
inline constexpr uint16_t MIN_FORMAT_VERSION = 1;

/// Why a harmonic ended up with the value it has. Values are part of the
/// PrusaLink dump and the file format.
enum class Reason : uint8_t {
    kept = 0,
    ratio_too_high = 1,
    below_min_size = 2,
    tuning_error = 3,
    verification_error = 4,
};

struct HarmonicVerification {
    /// residual(new) / residual(zero) per run; NaN when not measured.
    std::array<float, VERIFY_RUNS> ratio { NAN, NAN };
    /// The harmonic took part in the calibration.
    bool calibrated = false;
    bool kept = false;
    Reason reason = Reason::kept;
    /// Inactive unless the speed calibration passed its self-gate.
    speed_model::HarmonicModel speed_model {};
};

/// Indexed by harmonic.
using DirectionVerification = std::array<HarmonicVerification, opts::CORRECTION_HARMONICS + 1>;

struct MotorVerification {
    /// 0 = no record (nothing loaded and no calibration since boot).
    uint16_t version = 0;
    /// 0 = forward, 1 = backward.
    std::array<DirectionVerification, 2> direction {};
    /// Heap minimum of the calibration that produced the record.
    diagnostics::CalibrationHeap heap {};
};

/// Decide whether a tuned correction of size @p magnitude is kept, given the
/// ratios measured for it. A non-finite ratio counts as too high.
HarmonicVerification judge(float magnitude, const std::array<float, VERIFY_RUNS> &ratio);

/// Record of the last calibration of @p axis, or the one loaded from its file.
/// Written by the calibration task; other tasks may read a partially updated
/// record.
const MotorVerification &record(AxisEnum axis);

/// Start a new record for @p axis (called when its calibration starts).
void begin(AxisEnum axis);

/// Store the result of one harmonic in the current record of @p axis.
void set(AxisEnum axis, int harmonic, const HarmonicVerification &forward, const HarmonicVerification &backward);

/// Store the speed models of one harmonic (after set()).
void set_speed_model(AxisEnum axis, int harmonic, const speed_model::HarmonicModel &forward, const speed_model::HarmonicModel &backward);

/// Deactivate the speed model of @p harmonic in @p direction (0 = forward)
/// and persist the record, e.g. after the harmonic was overwritten by hand.
void drop_speed_model(AxisEnum axis, int direction, int harmonic);

/// Persist the record of @p axis, with the current calibration heap minimum. Does nothing when there is no record.
void save(AxisEnum axis);

/// Load the record of @p axis. A missing, short or unknown-version file leaves
/// an empty record (version 0), never an error. A version 1 file loads without
/// speed models. Restores the calibration heap minimum when none is known yet.
void load(AxisEnum axis);

/// Delete the file of @p axis and clear its record.
void remove(AxisEnum axis);

} // namespace phase_stepping::calibration_verify
