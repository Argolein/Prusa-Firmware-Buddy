#include <optional>
#include "../../lib/Marlin/Marlin/src/gcode/gcode.h"
#include "../../../lib/Marlin/Marlin/src/module/motion.h"
#include "../../../lib/Marlin/Marlin/src/module/planner.h"
#include <module/endstops.h>

#include "PrusaGcodeSuite.hpp"
#include <stdint.h>
#include "bsod.h"
#include "calibration_z.hpp"
#include "client_response.hpp"
#include "printers.h"
#include <marlin_server.hpp>
#include <raii/auto_restore.hpp>
#include <utils/progress.hpp>

#include <option/has_loadcell.h>
#if HAS_LOADCELL()
    #include "loadcell.hpp"
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

static constexpr feedRate_t Z_CALIB_ALIGN_AXIS_FEEDRATE = 15.f; // mm/s
static constexpr float Z_CALIB_EXTRA_HIGHT = 5.f; // mm

#if PRINTER_IS_PRUSA_XL()

/// Feedrate of the final push into the top hard stop, on which the motors skip to align themselves
static constexpr feedRate_t Z_CALIB_ALIGN_AXIS_SLOW_FEEDRATE = 4.f; // mm/s

/// Nozzle clearance left behind, dock calibration crosses the bed right after this
static constexpr float Z_CALIB_SAFE_CLEARANCE = 10.f; // mm

void selftest::calib_Z([[maybe_unused]] bool move_down_after) {
    marlin_server::fsm_change(PhasesSelftest::CalibZ);

    // backup original acceleration/feedrates and reset defaults for calibration
    Temporary_Reset_Motion_Parameters mp;

    // Home XY first
    if (!GcodeSuite::G28_no_parser(true, true, false, { .only_if_needed = true, .z_raise = 0, .precise = false })) {
        return; // This can happen only during print, homing recovery should follow
    }

    // mark test as failed (so it will be failed after reset - disconnected cables can cause rsod)
    auto result = config_store().selftest_result.get();
    result.set_zalign(TestResult::failed);
    config_store().selftest_result.set(result);

    // Move the nozzle up and away from the bed
    do_homing_move(Z_AXIS, Z_CALIB_EXTRA_HIGHT, HOMING_FEEDRATE_INVERTED_Z, false, false);
    current_position.z = 0;
    sync_plan_position();
    // Needs to avoid nozzle cleaner, tool offset sensor, and whatever else can be mounted on the XL
    current_position.x = X_BED_SIZE / 2;
    current_position.y = Y_MIN_POS + 2;
    line_to_current_position();
    planner.synchronize();

    // The nozzle is parked off the bed, so the loadcell would never trigger here even with
    // a tool picked. Approach the top position over the whole travel on the stall endstop.
    if (!do_homing_move(Z_AXIS, -(Z_MAX_POS - Z_MIN_POS) - Z_CALIB_EXTRA_HIGHT, HOMING_FEEDRATE_INVERTED_Z, false, false)
        && !planner.draining()) {
        fatal_error(ErrCode::ERR_ELECTRO_HOMING_ERROR_Z);
    }
    current_position.z = Z_MIN_POS;
    sync_plan_position();

    // The position above is fabricated and the push below deliberately targets past the
    // software limit. Drop the homed flag first, otherwise apply_motion_limits crops it.
    set_axis_is_not_at_home(Z_AXIS);

    // Lower the bed and repeat the push slowly to align the motors
    do_blocking_move_to_z(Z_MIN_POS + Z_CALIB_EXTRA_HIGHT, Z_CALIB_ALIGN_AXIS_FEEDRATE);
    do_blocking_move_to_z(Z_MIN_POS - Z_CALIB_EXTRA_HIGHT, Z_CALIB_ALIGN_AXIS_SLOW_FEEDRATE);

    // Back off the hard stop, the axis is left unhomed and Z_MIN_POS is past nozzle contact
    do_blocking_move_to_z(Z_MIN_POS + Z_CALIB_SAFE_CLEARANCE, Z_CALIB_ALIGN_AXIS_FEEDRATE);

    // Store Z aligned
    result.set_zalign(TestResult::passed);
    config_store().selftest_result.set(result);
}
#else
static constexpr float AFTER_Z_CALIB_Z_POS = 50;

static void safe_move_down(bool show_ui) {
    DEPLOY_PROBE();

    // Move to AFTER_Z_CALIB_Z_POS with Z endstop enabled
    float target_Z = AFTER_Z_CALIB_Z_POS - TERN0(HAS_HOTEND_OFFSET, hotend_currently_applied_offset.z);

    AutoRestore _se(soft_endstops_enabled, false);
    TemporaryGlobalEndstopsState _ess(true);

    // Create an empty optional for the subscriber so we can conditionally instantiate it
    std::optional<Subscriber<>> cb;
    if (show_ui) {
        cb.emplace(marlin_server::idle_publisher,
            [&]() {
                ProgressPercent progress = ProgressSpan { 0, 100 }.map(to_normalized_progress(current_position.z, target_Z, marlin_vars().native_pos[MARLIN_VAR_INDEX_Z]));
                marlin_server::fsm_change(FSMAndPhase(ClientFSM::Selftest, GetPhaseIndex(PhasesSelftest::CalibZ)), { progress });
            });
    }

    if (do_homing_move(AxisEnum::Z_AXIS, target_Z - current_position.z, HOMING_FEEDRATE_INVERTED_Z)) {
        // endstop triggered, raise the nozzle
        current_position.z = Z_MIN_POS;
        sync_plan_position();
        move_z_after_probing();
    } else {
        current_position.z = target_Z;
        sync_plan_position();
    }

    STOW_PROBE();
}

void selftest::calib_Z(bool move_down_after, bool show_ui) {
    // mark test as failed (so it will be failed after reset)
    auto result = config_store().selftest_result.get();
    result.set_zalign(TestResult::failed);
    config_store().selftest_result.set(result);

    // backup original acceleration/feedrates and reset defaults for calibration
    static constexpr float def_feedrate[] = DEFAULT_MAX_FEEDRATE;
    static constexpr float def_accel[] = DEFAULT_MAX_ACCELERATION;
    float orig_max_feedrate = planner.settings.max_feedrate_mm_s[Z_AXIS];
    float orig_max_accel = planner.settings.max_acceleration_mm_per_s2[Z_AXIS];
    planner.set_max_feedrate(Z_AXIS, def_feedrate[Z_AXIS]);
    planner.set_max_acceleration(Z_AXIS, def_accel[Z_AXIS]);

    // Z axis lift
    if (show_ui) {
        marlin_server::fsm_change(PhasesSelftest::CalibZ);
    }
    endstops.enable(true); // Stall endstops need to be enabled manually as in G28
    if (!homeaxis(Z_AXIS, HOMING_FEEDRATE_INVERTED_Z, true)) {
        fatal_error(ErrCode::ERR_ELECTRO_HOMING_ERROR_Z);
    }
    endstops.not_homing();

    // push both Z axis few mm over HW limit to align motors
    const float target_Z = Z_MAX_POS + Z_CALIB_EXTRA_HIGHT;
    current_position.z = Z_MAX_POS;
    sync_plan_position();
    do_blocking_move_to_z(target_Z, Z_CALIB_ALIGN_AXIS_FEEDRATE);
    current_position.z = Z_MAX_POS;
    sync_plan_position();

    // move a little bit back to stabilize the motors
    do_blocking_move_to_z(Z_MAX_POS - 1, Z_CALIB_ALIGN_AXIS_FEEDRATE);

    if (move_down_after) {
        safe_move_down(show_ui);
    }

    // always set axis as unhomed (Z_MAX_POS is unreliable, Z_MIN_POS is not probed with homeaxis()!)
    set_axis_is_not_at_home(Z_AXIS);

    // restore original values
    planner.set_max_feedrate(Z_AXIS, orig_max_feedrate);
    planner.set_max_acceleration(Z_AXIS, orig_max_accel);

    // Store Z aligned
    result.set_zalign(TestResult::passed);
    config_store().selftest_result.set(result);
}
#endif

/** \addtogroup G-Codes
 * @{
 */

/**
 *### G162: Z Calibration <a href="https://reprap.org/wiki/G-code#G162:_Home_axes_to_maximum">G162: Home axes to maximum</a>
 *
 *#### Usage
 *
 *    G162 [ Z ]
 *
 *#### Parameters
 *
 *  - `Z` - Calibrate Z axis
 */

void PrusaGcodeSuite::G162() {
    if (parser.seen('Z')) {
#if HAS_PHASE_STEPPING()
        phase_stepping::EnsureDisabled ps_disabler;
#endif
        marlin_server::FSM_Holder holder { PhasesSelftest::CalibZ };
        selftest::calib_Z(PRINTER_IS_PRUSA_iX() ? false : true);
    }
}

/** @}*/
