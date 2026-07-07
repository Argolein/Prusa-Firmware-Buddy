/**
 * @file marlin_print_preview.cpp
 */

#include "marlin_print_preview.hpp"
#include <M73_PE.h>
#include "client_fsm_types.h"
#include "client_response.hpp"
#include "general_response.hpp"
#include "marlin_server.hpp"
#include <media_prefetch_instance.hpp>
#include "timing.h"
#include <feature/filament_sensor/filament_sensors_handler.hpp>
#include "filament.hpp"
#include "M70X.hpp"
#include <option/developer_mode.h>
#include "printers.h"
#include <Marlin/src/module/motion.h>
#include <option/has_mmu2.h>
#include <option/has_gui.h>
#include <option/has_wastebin_fill_tracking.h>
#if HAS_WASTEBIN_FILL_TRACKING()
    #include <feature/wastebin_watcher/wastebin_watcher.hpp>
#endif
#if HAS_GUI()
    #include "screen_menu_filament_changeall.hpp"
#endif
#if HAS_E2EE_SUPPORT()
    #include <e2ee/key.hpp>
#endif

#include <option/has_spool_join.h>
#if HAS_SPOOL_JOIN()
    #include <module/prusa/spool_join.hpp>
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include <option/has_selftest.h>
#if HAS_SELFTEST()
    #include <selftest_result_evaluation.hpp>
#endif

#include <config_store/store_instance.hpp>
#include "tools_mapping.hpp"
#include <bsod/bsod.h>
#include <module/prusa/tool_mapper.hpp>
#include <mmu2_toolchanger_common.hpp>
#include <common/gcode/gcode_info_scan.hpp>
#include <utils/variant_utils.hpp>

// would be nice to have option leave phase as it was
// something like std::pair<enum {delete, leave, has_value },PhasesPrintPreview>
// but not currently needed
std::optional<PhasesPrintPreview> IPrintPreview::getCorrespondingPhase(IPrintPreview::State state) {
    switch (state) {

    case State::inactive:
        return std::nullopt;

    case State::init:
    case State::loading:
        return PhasesPrintPreview::loading;

    case State::download_wait:
        return PhasesPrintPreview::download_wait;

    case State::preview_wait_user:
        return PhasesPrintPreview::main_dialog;

    case State::unfinished_selftest_wait_user:
        return PhasesPrintPreview::unfinished_selftest;

    case State::new_firmware_available_wait_user:
        return PhasesPrintPreview::new_firmware_available;

#if HAS_TOOL_MAPPING()
    case State::tools_mapping_wait_user:
        return PhasesPrintPreview::tools_mapping;
#endif

    case State::gcode_invalid_wait_user:
        return PhasesPrintPreview::gcode_incompatible_warning;

    case State::gcode_invalid_wait_user_abort:
        return PhasesPrintPreview::gcode_incompatible_fatal;

#if HAS_WASTEBIN_FILL_TRACKING()
    case State::wastebin_overfill_wait_user:
        return PhasesPrintPreview::wastebin_overfill_warning;
    case State::wastebin_emptying:
        return std::nullopt; // silent wait for M1986
#endif

    case State::filament_not_inserted_wait_user:
    case State::filament_not_inserted_load:
        return PhasesPrintPreview::filament_not_inserted;

#if HAS_MMU2()
    case State::mmu_filament_inserted_wait_user:
    case State::mmu_filament_inserted_unload:
        return PhasesPrintPreview::mmu_filament_inserted;
#endif
    case State::wrong_filament_wait_user:
    case State::wrong_filament_change:
        return PhasesPrintPreview::wrong_filament;

#if HAS_E2EE_SUPPORT()
    case State::untrusted_identity:
        return PhasesPrintPreview::untrusted_identity;
#endif
    case State::file_error_wait_user:
        return PhasesPrintPreview::file_error;

    case State::checks_done:
    case State::done:
        return std::nullopt;
    }
    return std::nullopt;
}

void IPrintPreview::ChangeState(State s) {
#if HAS_E2EE_SUPPORT()
    // If we are leaving the print preview early for any reason, we need to clean up the tmp trusted identities
    if (s == State::inactive) {
        e2ee::remove_temporary_identites();
    }
#endif
    state = s;
    if (s != State::checks_done && s != State::init) { // don't inform about this state, since it's an internal one meant to be as a skip-through to proper state
        setFsm(getCorrespondingPhase(state));
    }
}

void IPrintPreview::setFsm(std::optional<PhasesPrintPreview> wantedPhase) {
    if (wantedPhase.has_value()) {
        marlin_server::fsm_change(*wantedPhase);
    }
    phase = wantedPhase;
}

Response IPrintPreview::GetResponse() {
    return phase ? marlin_server::get_response_from_phase(*phase) : Response::_none;
}

bool PrintPreview::check_extruder_need_filament_load(uint8_t physical_extruder, uint8_t no_gcode_value, stdext::inplace_function<uint8_t(uint8_t)> gcode_extruder_getter) {
    auto gcode_extruder = gcode_extruder_getter(physical_extruder);
    if (gcode_extruder == no_gcode_value) {
        return false; // if this physical_extruder is not printing, no need to check its filament
    }

    if (!GCodeInfo::getInstance().get_extruder_info(gcode_extruder).used()) {
        return false;
    }

    FilamentSensorState extruder_state = GetExtruderFSensor(physical_extruder) ? GetExtruderFSensor(physical_extruder)->get_state() : FilamentSensorState::Disabled;
    FilamentSensorState side_state = GetSideFSensor(physical_extruder) ? GetSideFSensor(physical_extruder)->get_state() : FilamentSensorState::Disabled;

    return (extruder_state == FilamentSensorState::NoFilament) || (side_state == FilamentSensorState::NoFilament);
}

static bool check_extruder_need_filament_load_tools_mapping(uint8_t physical_extruder) {
    return PrintPreview::check_extruder_need_filament_load(physical_extruder, tools_mapping::no_tool, tools_mapping::to_gcode_tool);
}

IPrintPreview::State PrintPreview::stateFromFilamentPresence() const {

#if HAS_MMU2()
    if (FSensors_instance().HasMMU()) {
        if (!config_store().fsensor_enabled.get()) {
            return State::checks_done;
        }

        buddy::gcode_compatibility::CompatibilityReport report;
        report.generate_toolmapping_only({});

        // with MMU, its only possible to check that filament is properly unloaded, no check of filaments presence in each "tool"
        if (FSensors_instance().MMUReadyToPrint()) {
            return State::checks_done;
        } else if (GCodeInfo::getInstance().is_singletool_gcode()
            && FSensors_instance().WhereIsFilament() == MMU2::FilamentState::AT_FSENSOR
            // only allow this shortcut print start if filament type is matching
            && !report.failed(buddy::gcode_compatibility::VirtualToolCheck::filament_type) //
        ) {
            // beware: State::checks_done shows the toolmapping screen.
            // We don't want that in MMU mode printing a single material gcode with filament already loaded in the nozzle.
            // In such a case we want to avoid toolmapping and start the print right away instead -> set State::done and not checks_done
            return State::done;
        } else {
            // Request removal of the inserted filament, esp. when:
            // - starting a multi-material print
            // - singlematerial print's material type not matching the inserted filament
            return State::mmu_filament_inserted_wait_user;
        }
    } else
#endif
    {
        if (!config_store().fsensor_enabled.get()) {
            return stateFromFilamentType();
        }
        if (tools_mapping::is_tool_mapping_possible()) {
            return stateFromFilamentType(); // filament loaded/type checks are handled by the tools_mapping screen
        }

        // no MMU, do regular check of filament presence in each tool
        for (int8_t e = 0; e < EXTRUDERS; e++) { // e == physical_extruder
            if (check_extruder_need_filament_load_tools_mapping(e)) {
                return State::filament_not_inserted_wait_user;
            }
        }
        return stateFromFilamentType();
    }
}

static void queue_filament_load_gcodes() {
    // Queue load filament gcode for every tool that doesn't have filament loaded
    for (int8_t e = 0; e < EXTRUDERS; e++) { // e == physical_extruder
        auto gcode_extruder = tools_mapping::to_gcode_tool(e);
        if (gcode_extruder == tools_mapping::no_tool) {
            // if this physical extruder is not printing, no need to load anything for it
            continue;
        }

        // skip for tools that already have filament
        if (!check_extruder_need_filament_load_tools_mapping(e)) {
            continue;
        }

        const auto &extruder_info = GCodeInfo::getInstance().get_extruder_info(gcode_extruder);

        // pass filament type from gcode, so that user doesn't have to select filament type
        const char *filament_name = extruder_info.filament_name.data();

#if HOTENDS > 1
        // if printer has multiple hotends (eg: XL), preheat all that will be loaded to save time for user
        // We're loading a new filament, do not fallback into ad-hoc one -> extruder_index = std::nullopt
        const auto target_temp = FilamentType::from_name(filament_name).parameters().nozzle_temperature;
        thermalManager.setTargetHotend(target_temp, e);
#endif
        marlin_server::enqueue_gcode_printf("M701 S\"%s\" T%d W2", filament_name, e);
    }
}

static void queue_filament_change_gcodes() {
    buddy::gcode_compatibility::CompatibilityReport report;
    report.generate_toolmapping_only({});

    // Queue change filament gcode for every tool with mismatched filament type
    for (auto gcode_tool : GcodeToolIndex::all()) {
        // TODO: Consider spool join? Is it necessary? This will probbaly be pre-filled with defaults, so no spool join
        const auto virtual_tool = stdext::get_optional<VirtualToolIndex>(gcode_tool.to_virtual());
        if (!virtual_tool.has_value()) {
            continue;
        }

        if (!report.failed(buddy::gcode_compatibility::VirtualToolCheck::filament_type, *virtual_tool)) {
            continue;
        }

        // if printer has multiple hotends (eg: XL), preheat all that will be loaded to save time for user
        auto temp_old = config_store().get_filament_type(*virtual_tool).parameters().nozzle_temperature;
        thermalManager.setTargetHotend(temp_old, virtual_tool->to_physical());

        // pass filament type from gcode, so that user doesn't have to select filament type
        const char *filament_name = GCodeInfo::getInstance().get_extruder_info(gcode_tool).filament_name.data();

        // M1600 - change, R - add return option, U1 - Ask filament type if unknown, T - tool, Sxxx - preselect filament type, P - do not use toolmapping
        marlin_server::enqueue_gcode_printf("M1600 S\"%s\" T%d R U1 P", filament_name, virtual_tool->to_raw());
    }
}

IPrintPreview::State PrintPreview::stateFromFilamentType() const {
    if (tools_mapping::is_tool_mapping_possible()) {
        return State::checks_done; // filament loaded/type checks are handled by the tools_mapping screen
    }

    // Argo (single-tool collapse): a multi-filament gcode deliberately printed on a
    // single enabled tool will always mismatch filament types (many gcode filaments,
    // one loaded), so the wrong-filament screen would just block an intended mono
    // print. Skip it for that case only — a normal single-tool print (single-tool
    // gcode) still gets the wrong-filament warning.
    if (VirtualToolIndex::single_enabled_tool().has_value()
        && GCodeInfo::getInstance().UsedExtrudersCount() > 1) {
        return State::checks_done;
    }

    buddy::gcode_compatibility::CompatibilityReport report;
    report.generate_toolmapping_only({});

    if (report.failed(buddy::gcode_compatibility::VirtualToolCheck::filament_type)) {
        return State::wrong_filament_wait_user;
    }

    return State::checks_done;
}

void PrintPreview::tools_mapping_cleanup(bool leaving_to_print) {
    if (!leaving_to_print) {
        // stop preheating bed
        marlin_server::set_target_bed(0);
#if HAS_TOOL_MAPPING()
        tool_mapper.reset();
        spool_join.reset();
#endif
    }

#if PRINTER_IS_PRUSA_XL()
    // set dwarf leds to be handled 'normally'
    for (auto tool : PhysicalToolIndex::all()) {
        prusa_toolchanger.getTool(tool).set_cheese_led(); // Default LED config
        prusa_toolchanger.getTool(tool).set_status_led(); // Default status LED
    }
#endif
}

PrintPreview::Result PrintPreview::Loop() {
    if (GetState() == State::inactive) {
        return Result::Inactive;
    }

    const uint32_t curr_ms = ticks_ms();
    if (ticks_diff(curr_ms, last_run) < max_run_period_ms) {
        return stateToResult();
    }
    last_run = curr_ms;
    const Response response = GetResponse();

    auto &gcode_info = GCodeInfo::getInstance();

    switch (GetState()) {

    case State::inactive: // cannot be, but have it defined to enumerate all states
        return Result::Inactive;

    case State::init:
        gcode_info_scan::start_scan();

        // Reset print progress to 0. Need to be at this point because Connect is already starting to snitch the info.
        oProgressData.mInit();

        ChangeState(State::loading);
        if (skip_if_able > marlin_server::PreviewSkipIfAble::no) {
            // if skip print confirmation was requested, mark the print as started immediately.
            // If not, it will be started later when user clicks print
            return Result::MarkStarted;
        }
        break;

    case State::download_wait: {
        if (response == Response::Quit) {
            gcode_info_scan::cancel_scan();
            ChangeState(State::inactive);
            return Result::Abort;
        }

        if (gcode_info.has_error()) {
            ChangeState(State::file_error_wait_user);
            break;

        } else if (!gcode_info.can_be_printed()) {
            // Wait till we have downloaded enough
            break;
        }

        const auto prefetch_ready = marlin_server::media_prefetch.check_ready_to_start_print();
        if (prefetch_ready == MediaPrefetchManager::ReadyToStartPrintResult::needs_fetching) {
            // Make sure we have the prefetch buffer full before start the print
            // If we got into the "downloading" phase, do the prefetch checking here, because we're waiting for the file to download more
            marlin_server::media_prefetch.issue_fetch();
            break;

        } else if (prefetch_ready == MediaPrefetchManager::ReadyToStartPrintResult::error) {
            // This is a bit hacky way, but the error reporting is done through gcode_info, so we gotta put the error there.
            gcode_info.set_error(N_("The file is corrupt."));
            ChangeState(State::file_error_wait_user);
            break;
        }

        ChangeState(State::loading);
        break;
    }

    case State::loading: {
        if (gcode_info_scan::scan_start_result() == gcode_info_scan::ScanStartResult::not_started) {
            // Wait for the gcode scan to start
            break;
        }

        if (gcode_info.has_error()) {
            ChangeState(State::file_error_wait_user);
            break;

#if HAS_E2EE_SUPPORT()
        } else if (gcode_info.has_identity_info()) {
            ChangeState(State::untrusted_identity);
            break;
#endif
        } else if (!gcode_info.can_be_printed()) {
            // The file is not fully downloaded, wait till we have downloaded enough for printing
            ChangeState(State::download_wait);
            break;

        } else if (!gcode_info.is_loaded()) {
            // Wait for the gcode info to fully load
            break;
        }
        const auto prefetch_ready = marlin_server::media_prefetch.check_ready_to_start_print();
        if (prefetch_ready == MediaPrefetchManager::ReadyToStartPrintResult::needs_fetching) {
            // Make sure we have the prefetch buffer full before start the print
            // If we got into the "downloading" phase, do the prefetch checking here, because we're waiting for the file to download more
            marlin_server::media_prefetch.issue_fetch();
            break;

        } else if (prefetch_ready == MediaPrefetchManager::ReadyToStartPrintResult::error) {
            // This is a bit hacky way, but the error reporting is done through gcode_info, so we gotta put the error there.
            gcode_info.set_error(N_("The file is corrupt."));
            ChangeState(State::file_error_wait_user);
            break;
        }

#if HAS_SELFTEST()
        // We're ready to print now
        ChangeState((skip_if_able > marlin_server::PreviewSkipIfAble::no) ? stateFromSelftestCheck() : State::preview_wait_user);
#else
        // #error dead code found by automatic analyses (see BFW-5461)
        ChangeState(State::preview_wait_user);
#endif
        break;
    }

    case State::preview_wait_user:
        switch (response) {

        case Response::Continue: // no difference in state machine, some checks will not be run if tools_mapping is possible
        case Response::Print:
            ChangeState(stateFromSelftestCheck());
            if (skip_if_able == marlin_server::PreviewSkipIfAble::no) {
                // If print wasn't maked as started immediately, mark it now
                return Result::MarkStarted;
            }
            break;

        case Response::Back:
            ChangeState(State::inactive);
            return Result::Abort;

        default:
            break;
        }

        // Periodically kindly ask the prefetch thread to check if the file is still valid and update GCodeInfo::has_error
        if (ticks_diff(curr_ms, last_still_valid_check_ms) > 1000 && !still_valid_check_job.is_active()) {
            last_still_valid_check_ms = curr_ms;

            still_valid_check_job.issue([](AsyncJobExecutionControl &) {
                GCodeInfo::getInstance().check_still_valid();
            });
        }

        // Still check for file validity - file could be downloaded enough for the info to show,
        // but the transfer might fail and we want to prevent the user from start the print then
        if (gcode_info.has_error()) {
            ChangeState(State::file_error_wait_user);
            break;
        }
        break;

    case State::unfinished_selftest_wait_user:
        switch (response) {

        case Response::Ignore:
            ChangeState(stateFromUpdateCheck());
            break;

        case Response::Calibrate:
            marlin_server::request_calibrations_screen();
            // There is currently no way to keep the FSM open while GUI screen
            // such as `Calibrations & Tests` is being shown.
            // Let's just close the FSM and be done with it.
            ChangeState(State::inactive);
            return Result::Abort;

        case Response::_none:
            break;

        default:
            bsod_unreachable();
        }
        break;

    case State::new_firmware_available_wait_user:
        switch (response) {

        case Response::Continue:
            ChangeState(stateFromPrinterCheck());
            break;

        default:
            // TODO this should be handled more generally with a possibility to set timeout for specific state, but this should work for now and is MUCH easier
            if (ticks_ms() >= new_firmware_open_ms + new_firmware_timeout_ms) {
                ChangeState(stateFromPrinterCheck());
            }
            break;
        }
        break;

#if HAS_TOOL_MAPPING()
    case State::tools_mapping_wait_user:
        switch (response) {

        case Response::Abort:
            ChangeState(State::inactive);
            tools_mapping_cleanup();
            return Result::Abort;

        case Response::Print:
            tools_mapping_cleanup(true);
            ChangeState(State::done);
            break;

        default:
            break;
        }
        break;
#endif

    case State::gcode_invalid_wait_user:
    case State::gcode_invalid_wait_user_abort:
        switch (response) {

        case Response::Print:
            ChangeState(stateFromFilamentPresence());
            break;

        case Response::Abort:
            ChangeState(State::inactive);
            return Result::Abort;

        default:
            break;
        }
        break;

#if HAS_WASTEBIN_FILL_TRACKING()
    case State::wastebin_overfill_wait_user:
        switch (response) {

        case Response::Print:
            // Print anyway and keep mid-print monitoring on.
            ChangeState(stateFromFilamentPresence());
            break;

        case Response::Ignore:
            // Print anyway and stop monitoring for this print (e.g. remote user who will empty later).
            WastebinWatcher::instance().set_ignored_for_print(true);
            ChangeState(stateFromFilamentPresence());
            break;

        case Response::Empty:
            marlin_server::enqueue_gcode("M1986");
            ChangeState(State::wastebin_emptying);
            break;

        case Response::_none:
            break;

        default:
            bsod_unreachable();
        }
        break;

    case State::wastebin_emptying:
        // M1986 runs as a gcode; wait for the gcode queue to drain before continuing.
        if (marlin_server::is_processing()) {
            break;
        }
        ChangeState(stateFromFilamentPresence());
        break;
#endif

    case State::filament_not_inserted_wait_user:
        switch (response) {

        case Response::FS_disable:
            FSensors_instance().set_enabled_global(false);
            ChangeState(State::checks_done);
            break;

        case Response::No:
            ChangeState(State::inactive);
            return Result::Abort;

        case Response::Yes:
            ChangeState(State::filament_not_inserted_load);
            queue_filament_load_gcodes();
            break;

        default:
            break;
        }
        break;

    case State::filament_not_inserted_load:
        if (!filament_gcodes::InProgress::Active()) {
            ChangeState(stateFromFilamentType());
        }
        break;

#if HAS_MMU2()
    case State::mmu_filament_inserted_wait_user:
        switch (response) {

        case Response::No:
            ChangeState(State::inactive);
            return Result::Inactive;

        case Response::Yes: {

            ChangeState(State::mmu_filament_inserted_unload);
            // If filament is really loaded, MMU knows exactly the physical slot index.
            // Silently assume, that the MMU is up and running -> therefore it's Set_Get_Selector_Slot register has already been read upon comm start
            const uint8_t physical_slot_index = MMU2::mmu2.SelectorSlot();
            // M702 translates given tool index through toolmapping -> need to hack around it (invert it).
            // The tool_mapper should either return the same index in case of a freshly booted printer (or no mapping changes)
            // or a valid mapping from the last print (which is also correct in this case).
            const uint8_t gcode_slot_index = tool_mapper.to_gcode(physical_slot_index);
            marlin_server::enqueue_gcode_printf("M702 W0 T%" PRIu8, gcode_slot_index); // unload, no return or cooldown
        } break;

        default:
            break;
        }
        break;

    case State::mmu_filament_inserted_unload:
        if (!filament_gcodes::InProgress::Active()) {
            ChangeState(State::checks_done);
        }
        break;
#endif

    case State::wrong_filament_wait_user: // change / ignore / abort
        switch (response) {

        case Response::Change:
            ChangeState(State::wrong_filament_change);
            queue_filament_change_gcodes();
            break;

        case Response::Ok:
            ChangeState(State::checks_done);
            break;

        case Response::Abort:
            ChangeState(State::inactive);
            return Result::Abort;

        default:
            break;
        }
        break;

    case State::wrong_filament_change:
        if (!filament_gcodes::InProgress::Active()) {
            PreheatStatus::Result res = PreheatStatus::ConsumeResult();
            if (res == PreheatStatus::Result::Aborted || res == PreheatStatus::Result::DidNotFinish) {
                ChangeState(State::wrong_filament_wait_user); // Return back to wrong filament type dialog
            } else {
                ChangeState(State::checks_done);
            }
        }
        break;

#if HAS_E2EE_SUPPORT()
    case State::untrusted_identity:
        if (response == Response::Abort) {
            ChangeState(State::inactive);
            return Result::Abort;
        } else if (response == Response::Yes) {
            const auto &info = gcode_info.get_identity_info();
            if (info.one_time) {
                e2ee::save_identity_key_temporary(info);
            } else {
                e2ee::save_identity_key(info);
            }
            ChangeState(State::preview_wait_user);
            break;
        } else if (response == Response::No) {
            e2ee::save_identity_key_temporary(gcode_info.get_identity_info());
            ChangeState(State::preview_wait_user);
            break;
        }
        break;
#endif

    case State::file_error_wait_user:
        // Only one possible response -> abort
        if (response == Response::Abort) {
            ChangeState(State::inactive);
            return Result::Abort;
        }
        break;

    case State::checks_done:
#if PRINTER_IS_PRUSA_iX()
        // We've removed reset_bounding_rect at the end of the print for the iX (in marlin_server.cpp::finalize_print).
        // So now, just to make sure, we reset the bounding rect at the start if we don't see it being set in the gcode.
        // BFW-5085
        if (!GCodeInfo::getInstance().get_bed_preheat_area().has_value()) {
            PrintArea::reset_bounding_rect();
        }
#endif

        if (tools_mapping::is_tool_mapping_possible()) {
#if HAS_SPOOL_JOIN() && HAS_TOOL_MAPPING()
            buddy::gcode_compatibility::CompatibilityReport compatibility;
            compatibility.generate_toolmapping_only({});

            // we can skip tools mapping if there is not warning/error in global tools mapping
            if ((skip_if_able >= marlin_server::PreviewSkipIfAble::tool_mapping) && compatibility.failure_severity() == HWCheckSeverity::Ignore) {
                ChangeState(State::done);
            } else {
                ChangeState(State::tools_mapping_wait_user);
            }

            // start preheating bed to save time in absorbing heat
            if (GCodeInfo::getInstance().get_bed_preheat_temp().has_value()) {
                if (GCodeInfo::getInstance().get_bed_preheat_area().has_value()) {
                    PrintArea::set_bounding_rect(GCodeInfo::getInstance().get_bed_preheat_area().value());
                }

                marlin_server::set_target_bed(GCodeInfo::getInstance().get_bed_preheat_temp().value());
            }
            break;
#endif
        }
        // else go to print, checks are done
        ChangeState(State::done);
        break;
    case State::done:
        ChangeState(State::inactive);
        return Result::Print;
    }
    return stateToResult();
}

PrintPreview::Result PrintPreview::stateToResult() const {
    switch (GetState()) {

    case State::init:
    case State::download_wait:
    case State::loading:
        return Result::Wait;

    case State::preview_wait_user:
        return Result::Image;

    case State::unfinished_selftest_wait_user:
    case State::new_firmware_available_wait_user:
    case State::gcode_invalid_wait_user:
    case State::gcode_invalid_wait_user_abort:
#if HAS_WASTEBIN_FILL_TRACKING()
    case State::wastebin_overfill_wait_user:
    case State::wastebin_emptying:
#endif
    case State::wrong_filament_change:
    case State::wrong_filament_wait_user:
    case State::filament_not_inserted_load:
    case State::filament_not_inserted_wait_user:
#if HAS_MMU2()
    case State::mmu_filament_inserted_unload:
    case State::mmu_filament_inserted_wait_user:
#endif
    case State::checks_done:
#if HAS_E2EE_SUPPORT()
    case State::untrusted_identity:
#endif
    case State::file_error_wait_user:
        return Result::Questions;

    case State::inactive:
    case State::done:
        return Result::Inactive;

#if HAS_TOOL_MAPPING()
    case State::tools_mapping_wait_user:
        return Result::ToolsMapping;
#endif
    }
    return Result::Inactive;
}

void PrintPreview::Init() {
    ChangeState(State::init);
}

IPrintPreview::State PrintPreview::stateFromSelftestCheck() {
#if HAS_SELFTEST()
    if (!is_selftest_successfully_completed()) {
        return State::unfinished_selftest_wait_user;
    }
#endif
    return stateFromUpdateCheck();
}

IPrintPreview::State PrintPreview::stateFromUpdateCheck() {
    if (GCodeInfo::getInstance().info().new_firmware_available && config_store().hw_check_firmware.get() != HWCheckSeverity::Ignore) {
        new_firmware_open_ms = ticks_ms();
        return State::new_firmware_available_wait_user;
    } else {
        return stateFromPrinterCheck();
    }
}

#if HAS_WASTEBIN_FILL_TRACKING()
IPrintPreview::State PrintPreview::stateFromWastebinCheck() {
    // Fresh decision for every print: re-enable mid-print monitoring so a previous print's
    // "Ignore" cannot leak into this one. Pressing Ignore on the warning below re-disables it.
    WastebinWatcher::instance().set_ignored_for_print(false);

    if (WastebinWatcher::instance().print_will_overfill()) {
        return State::wastebin_overfill_wait_user;
    }
    return stateFromFilamentPresence();
}
#endif

IPrintPreview::State PrintPreview::stateFromPrinterCheck() {
    // Determine whether we want to show the incompatibilities screen

#if HAS_TOOL_MAPPING()
    // Singletool gcode with exactly one enabled tool: auto-remap T0 to that tool
    if (!tool_mapper.is_enabled()
        && GCodeInfo::getInstance().is_singletool_gcode()) {
        if (auto single = VirtualToolIndex::single_enabled_tool();
            single.has_value() && single->to_raw() != 0) {
            tool_mapper.reset();
            tool_mapper.set_mapping(GcodeToolIndex::from_raw(0), *single);
            tool_mapper.set_enable(true);
        }
    }
#endif

    buddy::gcode_compatibility::CompatibilityReport report;
    if (tools_mapping::is_tool_mapping_possible()) {
        // Only check non-tool related things
        // Tool checks will be handled on the tool mapping screen
        report.generate_without_toolmapping();

    } else {
        // There will be no separate toolmapping screen,
        // so show all problems, with the current toolmapping
        report.generate_full({});
    }

    // Ignore wrong filament types here, they are handled separately in stateFromFilamentType
    for (auto &vtd : report.failed_virtual_tool_checks) {
        vtd.reset(buddy::gcode_compatibility::VirtualToolCheck::filament_type);
    }

    switch (report.failure_severity()) {

    case HWCheckSeverity::Ignore:
#if HAS_WASTEBIN_FILL_TRACKING()
        return stateFromWastebinCheck();
#else
        return stateFromFilamentPresence();
#endif

    case HWCheckSeverity::Warning:
        return State::gcode_invalid_wait_user;

    case HWCheckSeverity::Abort:
        return State::gcode_invalid_wait_user_abort;
    }

    bsod_unreachable();
}
