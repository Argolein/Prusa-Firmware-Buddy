#pragma once

#include <option/has_dwarf.h>
#include <option/has_esp.h>
#include <option/has_gearbox_alignment.h>
#include <option/has_loadcell.h>
#include <option/has_selftest.h>
#include <option/has_phase_stepping_calibration.h>
#include <option/has_coldpull.h>
#include <option/has_input_shaper_calibration.h>
#include <option/has_side_fsensor.h>
#include <option/has_emergency_stop.h>
#include <option/xl_enclosure_support.h>
#include <option/has_chamber_api.h>
#include <option/has_uneven_bed_prompt.h>
#include <option/has_door_sensor_calibration.h>
#include <option/has_z_endstop_calibration.h>
#include <option/has_manual_belt_tuning.h>
#include <option/has_serial_print.h>
#include <option/has_indx.h>

#include <inc/MarlinConfigPre.h>

#include <stdint.h>
#include <utils/utility_extensions.hpp>

#ifdef __cplusplus
// C++ checks enum classes

// Client finite state machines
// bound to src/client_response.hpp
enum class ClientFSM : uint8_t {
    #if HAS_SERIAL_PRINT()
    Serial_printing,
    #endif
    Load_unload,
    Preheat,
    #if HAS_SELFTEST()
    Selftest,
    FansSelftest,
    SelftestFSensors,
    #endif
    #if HAS_ESP()
    NetworkSetup,
    #endif
    Printing, // not a dialog
    #if ENABLED(CRASH_RECOVERY)
    CrashRecovery,
    #endif
    QuickPause,
    Warning,
    PrintPreview,
    #if HAS_COLDPULL()
    ColdPull,
    #endif
    #if HAS_MANUAL_BELT_TUNING()
    ManualBeltTuning,
    #endif
    #if HAS_PHASE_STEPPING_CALIBRATION()
    PhaseSteppingCalibration,
    #endif
    #if HAS_INPUT_SHAPER_CALIBRATION()
    InputShaperCalibration,
    #endif
    #if HAS_GEARBOX_ALIGNMENT()
    GearboxAlignment,
    #endif
    #if HAS_DOOR_SENSOR_CALIBRATION()
    DoorSensorCalibration,
    #endif
    #if HAS_Z_ENDSTOP_CALIBRATION()
    ZEndstopCalibration,
    #endif
    #if HAS_LOADCELL()
    NozzleCleaningFailed,
    #endif
    #if HAS_INDX()
    NozzleMismatch,
    DockCalibration,
    ToolOffsetsCalibration,
    NozzleCleanerCalibration,
    #endif
    SafetyTimer,
    Wait, ///< FSM that only blocks the screen with a "please wait" text
    _none, // cannot be created, must have same index as _count
    _count = _none
};

// We have only 5 bits for it in the serialization of data sent between server and client
static_assert(std::to_underlying(ClientFSM::_count) < 32);

enum class LoadUnloadMode : uint8_t {
    Change,
    Load,
    Unload,
    Purge,
    FilamentStuck,
    Test,
    Cut, // MMU
    Eject, // MMU
};

enum class PreheatMode : uint8_t {
    /// Selecting filament just for preheating (from the menu)
    preheat,

    /// Explicitly triggered standard load
    standard_load,

    /// Load during a filament change procedure (preceded by unload)
    change_load,

    /// Automatically triggered load (typically by inserting the filament to the fsensor)
    autoload,

    unload,
    purge,
    _last = purge
};

enum class RetAndCool_t : uint8_t {
    Neither = 0b00,
    Cooldown = 0b01,
    Return = 0b10,
    Both = 0b11,
    last_ = Both
};

#else // !__cplusplus
// #error dead code found by automatic analyses (see BFW-5461)
// C
#endif //__cplusplus
