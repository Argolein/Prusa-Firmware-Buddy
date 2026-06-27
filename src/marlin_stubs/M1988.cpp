#include "M1988.hpp"

#include <option/has_z_endstop_calibration.h>
static_assert(HAS_Z_ENDSTOP_CALIBRATION());

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

#include <bsod/bsod.h>
#include <common/marlin_server.hpp>
#include <client_response.hpp>
#include <config_store/store_instance.hpp>
#include <test_result.hpp>
#include <z_endstop_calibration_result.hpp>

#include "calibration_z.hpp"

#include <Marlin/src/Marlin.h> // z_auto_align_done
#include <Marlin/src/gcode/gcode.h>
#include <Marlin/src/inc/MarlinConfig.h>
#include <Marlin/src/module/motion.h>
#include <Marlin/src/module/probe.h>

using marlin_server::wait_for_response;

namespace {

struct ProbePoint {
    float x;
    float y;
};

// One point closest to each of the three Z motors. Coordinates derive from the mesh-probeable
// area so they stay valid if the bed size / grid changes. "Front" is Y_min (door side):
//   front-left  = (MESH_MIN_X, MESH_MIN_Y)
//   rear-center = (center,     MESH_MAX_Y)
//   front-right = (MESH_MAX_X, MESH_MIN_Y)
// The order matches ZEndstopCalibrationPoint.
constexpr ProbePoint probe_points[] = {
    { MESH_MIN_X, MESH_MIN_Y },
    { (MESH_MIN_X + MESH_MAX_X) / 2.f, MESH_MAX_Y },
    { MESH_MAX_X, MESH_MIN_Y },
};
static_assert(std::size(probe_points) == static_cast<size_t>(ZEndstopCalibrationPoint::count_));

PhasesZEndstopCalib do_homing() {
    marlin_server::fsm_change(PhasesZEndstopCalib::homing);

    // Home X/Y only (no Z, so G28's auto-align branch is not entered here).
    if (!GcodeSuite::G28_no_parser(true, true, false, { .precise = false })) {
        return PhasesZEndstopCalib::probe_failed;
    }
    return PhasesZEndstopCalib::aligning;
}

PhasesZEndstopCalib do_aligning() {
    marlin_server::fsm_change(PhasesZEndstopCalib::aligning);

    // Run the Z alignment explicitly on every iteration. We must NOT rely on G28's auto-align:
    // it only fires once per power cycle (gated by z_auto_align_done) and would be skipped on
    // retries. calib_Z rams the gantry to the top against the adjustable endstops and leaves Z
    // unhomed.
    selftest::calib_Z(false, false);

    // Home Z for an absolute probing reference, but suppress G28's auto-align so it does not run
    // a second alignment. Restore the flag afterwards so a later print homes/aligns normally.
    z_auto_align_done = true;
    const bool z_homed = GcodeSuite::G28_no_parser(false, false, true, { .precise = false });
    z_auto_align_done = false;
    if (!z_homed) {
        return PhasesZEndstopCalib::probe_failed;
    }
    return PhasesZEndstopCalib::probing;
}

PhasesZEndstopCalib do_probing(ZEndstopCalibResult_t &result) {
    marlin_server::fsm_change(PhasesZEndstopCalib::probing);

    for (size_t i = 0; i < std::size(probe_points); ++i) {
        const xy_pos_t pos = { probe_points[i].x, probe_points[i].y };
        const float z = probe_at_point(pos, PROBE_PT_RAISE);
        if (std::isnan(z)) {
            return PhasesZEndstopCalib::probe_failed;
        }
        result.z[i] = z;
    }

    const auto [min_it, max_it] = std::minmax_element(result.z.begin(), result.z.end());
    result.spread = *max_it - *min_it;

    // Persist pass/fail so the menu entry can show its green "OK" icon.
    config_store().selftest_result_z_endstop_calibration.set(
        result.spread <= z_endstop_calib_tolerance_mm ? TestResult::passed : TestResult::failed);

    return PhasesZEndstopCalib::show_result;
}

PhasesZEndstopCalib do_show_result(const ZEndstopCalibResult_t &result) {
    marlin_server::fsm_change_extended(PhasesZEndstopCalib::show_result, result);
    switch (wait_for_response(PhasesZEndstopCalib::show_result)) {
    case Response::Retry:
        return PhasesZEndstopCalib::homing;
    case Response::Quit:
        return PhasesZEndstopCalib::finish;
    default:
        bsod_unreachable();
    }
}

PhasesZEndstopCalib do_probe_failed() {
    marlin_server::fsm_change(PhasesZEndstopCalib::probe_failed);
    switch (wait_for_response(PhasesZEndstopCalib::probe_failed)) {
    case Response::Retry:
        return PhasesZEndstopCalib::homing;
    case Response::Quit:
        return PhasesZEndstopCalib::finish;
    default:
        bsod_unreachable();
    }
}

} // namespace

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1988: Z endstop calibration wizard
 *
 * Internal GCode (Core One family). Homes, runs the Z alignment and probes one point closest to
 * each of the three Z motors, then shows the measured heights and their spread so the user can
 * trim the adjustable Z endstops and repeat.
 *
 *#### Usage
 *
 *    M1988
 *
 */
void PrusaGcodeSuite::M1988() {
    ZEndstopCalibResult_t result;
    PhasesZEndstopCalib phase = PhasesZEndstopCalib::homing;

    marlin_server::FSM_Holder holder { phase };
    while (phase != PhasesZEndstopCalib::finish) {
        switch (phase) {
        case PhasesZEndstopCalib::homing:
            phase = do_homing();
            break;
        case PhasesZEndstopCalib::aligning:
            phase = do_aligning();
            break;
        case PhasesZEndstopCalib::probing:
            result = ZEndstopCalibResult_t {}; // discard any previous heights before re-probing
            phase = do_probing(result);
            break;
        case PhasesZEndstopCalib::show_result:
            phase = do_show_result(result);
            break;
        case PhasesZEndstopCalib::probe_failed:
            phase = do_probe_failed();
            break;
        case PhasesZEndstopCalib::finish:
            break;
        }
    }
}

/** @}*/
