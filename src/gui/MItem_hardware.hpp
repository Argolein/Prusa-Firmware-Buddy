#pragma once
#include "WindowMenuItems.hpp"
#include "i18n.h"
#include <config_store/store_instance.hpp>
#include <option/has_side_fsensor_remap.h>
#include <option/has_toolchanger.h>
#include <option/has_emergency_stop.h>
#include <option/has_chamber_vents.h>
#include <common/extended_printer_type.hpp>
#include <gui/menu_item/menu_item_select_menu.hpp>
#include <option/has_side_fsensor_remap.h>

class MI_HARDWARE_CHECK : public MenuItemSwitch {
public:
    MI_HARDWARE_CHECK(HWCheckType check_type);

protected:
    void OnChange([[maybe_unused]] size_t old_index) final;

private:
    const HWCheckType check_type;
};

class MI_HARDWARE_G_CODE_CHECKS : public IWindowMenuItem {
    static constexpr const char *const label = N_("G-Code Checks");

public:
    MI_HARDWARE_G_CODE_CHECKS();

protected:
    virtual void click(IWindowMenu &windowMenu) override;
};

#if HAS_SIDE_FSENSOR_REMAP()
class MI_SIDE_FSENSOR_REMAP : public WI_ICON_SWITCH_OFF_ON_t {
    static constexpr const char *const label = N_("Side FSensor Remap");

public:
    MI_SIDE_FSENSOR_REMAP();

protected:
    virtual void OnChange(size_t old_index) override;
};
#endif

#if HAS_EXTENDED_PRINTER_TYPE()
class MI_EXTENDED_PRINTER_TYPE : public MenuItemSelectMenu {
public:
    MI_EXTENDED_PRINTER_TYPE();

    int item_count() const final;
    string_view_utf8 build_item_text(int index, ItemTextParams &params) const final;

protected:
    bool on_item_selected(const OnItemSelectedArgs &args) override;
};

#else
class MI_EXTENDED_PRINTER_TYPE : public IWiInfo {
public:
    MI_EXTENDED_PRINTER_TYPE()
        : IWiInfo(string_view_utf8::MakeCPUFLASH(PrinterModelInfo::current().id_str), _("Printer Type")) {}
};

#endif

#if HAS_EMERGENCY_STOP()
class MI_EMERGENCY_STOP_ENABLE : public WI_ICON_SWITCH_OFF_ON_t {
    static constexpr const char *const label = N_("Door Sensor");

public:
    MI_EMERGENCY_STOP_ENABLE();

protected:
    virtual void OnChange(size_t old_index) override;
};
#endif

#if HAS_CHAMBER_VENTS()
class MI_SWITCH_VENT_MECHANISM : public MenuItemSwitch {
public:
    MI_SWITCH_VENT_MECHANISM();

protected:
    virtual void OnChange(size_t old_index) override;
};
#endif

#if HAS_PRECISE_HOMING_COREXY()
/// Option whether we should automatically calibrate precise homing when needed
class MI_AUTO_PRECISE_HOMING_CALIBRATION : public MenuItemSwitch {
public:
    MI_AUTO_PRECISE_HOMING_CALIBRATION();

protected:
    virtual void OnChange(size_t) override;
};
#endif

/// Master enable for the Filament Color Manager. Disabling it wipes all stored colors.
class MI_FILAMENT_COLOR_MANAGER : public WI_ICON_SWITCH_OFF_ON_t {
    static constexpr const char *const label = N_("Filament Color Manager");

public:
    MI_FILAMENT_COLOR_MANAGER();

protected:
    virtual void OnChange(size_t old_index) override;
};
