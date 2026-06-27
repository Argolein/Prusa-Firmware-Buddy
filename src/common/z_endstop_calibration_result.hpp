#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "marlin_server_extended_fsm_types.hpp"

/**
 * @brief Result of the Z endstop calibration, passed from the marlin server to the GUI client.
 *
 * Holds the measured Z height closest to each of the three Z motors plus the spread
 * (max - min). The point order matches ZEndstopCalibrationPoint. Heights are absolute machine Z
 * relative to the Z homing reference; NaN marks a point that has not been (successfully) probed.
 */
enum class ZEndstopCalibrationPoint : uint8_t {
    front_left = 0,
    rear_center = 1,
    front_right = 2,
    count_ = 3,
};

/// Spread (max - min) at or below this counts as aligned ("green OK"). [mm]
inline constexpr float z_endstop_calib_tolerance_mm = 0.10f;

/// Axial travel per full turn of the adjustable endstop screw (standard M3 coarse thread); the
/// corner height moves ~1:1 with the screw, so turns = ΔZ / pitch. [mm/turn]
inline constexpr float z_endstop_screw_pitch_mm = 0.5f;

struct ZEndstopCalibResult_t : public FSMExtendedData {
    std::array<float, static_cast<size_t>(ZEndstopCalibrationPoint::count_)> z { { NAN, NAN, NAN } };
    float spread = NAN; ///< max(z) - min(z) [mm]

    constexpr ZEndstopCalibResult_t() = default;
    bool operator==(const ZEndstopCalibResult_t &) const = default;
};
