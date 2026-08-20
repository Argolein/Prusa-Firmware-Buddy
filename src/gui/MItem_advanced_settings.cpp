/**
 * @file MItem_advanced_settings.cpp
 */

#include "MItem_advanced_settings.hpp"

MI_ADV_PREHEAT_FOR_UNLOADING::MI_ADV_PREHEAT_FOR_UNLOADING()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().preheat_for_unloading.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_PREHEAT_FOR_UNLOADING::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().preheat_for_unloading.set(value());
}

MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE::MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().cooldown_after_loading_when_idle.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_COOLDOWN_AFTER_LOADING_WHEN_IDLE::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().cooldown_after_loading_when_idle.set(value());
}

MI_ADV_AUTO_Z_ALIGN_HOMING::MI_ADV_AUTO_Z_ALIGN_HOMING()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().auto_z_align_homing.get() ? 1 : 0, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_ADV_AUTO_Z_ALIGN_HOMING::OnChange(size_t old_index) {
    WI_ICON_SWITCH_OFF_ON_t::OnChange(old_index);
    config_store().auto_z_align_homing.set(value());
}
