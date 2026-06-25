/// @file
#pragma once

#include <marlin_server_types/client_response.hpp>

#include <option/has_anfc.h>

enum class PhasesPreheat : PhaseUnderlyingType {
    user_temp_selection,

    /// Filament Color Manager: after the filament type is chosen during a load,
    /// the user picks a color from the predefined palette. The chosen color is
    /// passed back through FSMResponseVariant(Color). Only used when the feature
    /// is enabled and the preheat is part of a load.
    user_color_selection,

#if HAS_ANFC()
    /// Asks the user to load the data from OpenPrintTag
    ask_load_openprinttag,

    /// Shows a screen with filament parameters loaded from the OpenPrintTag,
    /// allowing the user to adjust them
    /// This is typically necessary when the parameters on the tag are incomplete.
    /// !!! Currently, this is implemented as a hack where the GUI opens ScreenOPTFilamentDetail and immediately sends Response::Ok
    /// !!! This then switches the FSM to user_temp_selection; the OPT is handled in a standard PendingAdHocFilament way
    openprinttag_parameters,
#endif

    _cnt,
    _last = _cnt - 1
};

namespace ClientResponses {

inline constexpr EnumArray<PhasesPreheat, PhaseResponses, PhasesPreheat::_cnt> preheat_responses {
    // Additionally, filament type selection is passed through FSMResponseVariant(FilamentType)
    { PhasesPreheat::user_temp_selection, { Response::Abort, Response::Cooldown } },

        // Additionally, the chosen color is passed through FSMResponseVariant(Color)
        { PhasesPreheat::user_color_selection, { Response::Abort } },

#if HAS_ANFC()
        { PhasesPreheat::ask_load_openprinttag, { Response::Yes, Response::No, Response::Always, Response::Never } },
        { PhasesPreheat::openprinttag_parameters, {} },
#endif
};

} // namespace ClientResponses

constexpr inline ClientFSM client_fsm_from_phase(PhasesPreheat) { return ClientFSM::Preheat; }
