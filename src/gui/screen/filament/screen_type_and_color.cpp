#include "screen_type_and_color.hpp"

#include <algorithm_extensions.hpp>
#include <config_store/store_instance.hpp>
#include <display.hpp>
#include <guiconfig/GuiDefaults.hpp>
#include <print_utils.hpp>
#include <ScreenHandler.hpp>
#include <utils/color.hpp>
#include <utils/string_builder.hpp>

using namespace screen_type_and_color;

// * MI_FILAMENT_TYPE_SELECT
MI_FILAMENT_TYPE_SELECT::MI_FILAMENT_TYPE_SELECT()
    : MenuItemSelectMenu(_("Filament Type")) {
    rebuild();
}

void MI_FILAMENT_TYPE_SELECT::set_tool(VirtualToolIndex tool) {
    tool_ = tool;
    rebuild();
}

void MI_FILAMENT_TYPE_SELECT::rebuild() {
    const FilamentType current = FilamentType::for_tool(tool_);

    // enforce_first_item makes sure the current type is present in the list even if hidden
    generate_filament_list(filament_list_, { .enforce_first_item = current });

    // Index 0 is the "None" entry, the filament list follows.
    if (current == FilamentType::none) {
        set_current_item(0);
    } else {
        set_current_item(static_cast<int>(stdext::index_of(filament_list_, current)) + 1);
    }
}

int MI_FILAMENT_TYPE_SELECT::item_count() const {
    return static_cast<int>(filament_list_.size()) + 1; // + "None"
}

string_view_utf8 MI_FILAMENT_TYPE_SELECT::build_item_text(int index, ItemTextParams &params) const {
    if (index == 0) {
        return _("None");
    }

    StringBuilder sb(params.buffer);
    sb.append_string(filament_list_[index - 1].parameters().name.data());
    return string_view_utf8::MakeRAM(params.buffer.data());
}

bool MI_FILAMENT_TYPE_SELECT::on_item_selected(const OnItemSelectedArgs &args) {
    const FilamentType new_type = (args.new_index == 0) ? FilamentType::none : filament_list_[args.new_index - 1];

    // Setting the type to none clears the stored color as well (handled in set_filament_type).
    config_store().set_filament_type(tool_, new_type);
    return true;
}

// * MI_FILAMENT_COLOR_SELECT
MI_FILAMENT_COLOR_SELECT::MI_FILAMENT_COLOR_SELECT()
    : MenuItemSelectMenu(_("Filament Color")) {
    set_current_item(0);
}

void MI_FILAMENT_COLOR_SELECT::set_tool(PhysicalToolIndex tool) {
    tool_ = tool;
    const std::optional<uint8_t> stored = config_store().get_filament_color(tool_);
    set_current_item(stored ? static_cast<int>(*stored) + 1 : 0);
}

int MI_FILAMENT_COLOR_SELECT::item_count() const {
    return static_cast<int>(filament_color_presets.size()) + 1; // + "None"
}

string_view_utf8 MI_FILAMENT_COLOR_SELECT::build_item_text(int index, ItemTextParams &params) const {
    if (index == 0) {
        return _("None");
    }

    StringBuilder sb(params.buffer);
    sb.append_string_view(string_view_utf8::MakeCPUFLASH(filament_color_presets[index - 1].name.data()));
    return string_view_utf8::MakeRAM(params.buffer.data());
}

bool MI_FILAMENT_COLOR_SELECT::on_item_selected(const OnItemSelectedArgs &args) {
    const std::optional<uint8_t> palette_index = (args.new_index == 0)
        ? std::nullopt
        : std::optional<uint8_t>(static_cast<uint8_t>(args.new_index - 1));

    config_store().set_filament_color(tool_, palette_index);
    return true;
}

void MI_FILAMENT_COLOR_SELECT::printExtension(Rect16 extension_rect, [[maybe_unused]] Color color_text, Color color_back, [[maybe_unused]] ropfn raster_op) const {
    const int item = current_item();
    if (item <= 0) {
        // "None" -> draw nothing, leaving the swatch area blank.
        return;
    }

    // Draw a rounded color swatch, mirroring the tool-mapping screen.
    const Color color = filament_color_presets[item - 1].color;
    constexpr auto margin = 6;
    constexpr auto padding = 1;

    const auto outer_size = extension_rect.Height() - margin * 2;
    const Rect16 outer_rect = Rect16::fromLTWH(extension_rect.Right() - outer_size, extension_rect.Top() + margin, outer_size, outer_size);
    display::draw_rounded_rect(outer_rect, color_back, COLOR_WHITE, GuiDefaults::MenuItemCornerRadius, MIC_ALL_CORNERS);

    const auto inner_size = outer_size - padding * 2;
    const Rect16 inner_rect = Rect16::fromLTWH(outer_rect.Left() + padding, outer_rect.Top() + padding, inner_size, inner_size);
    display::draw_rounded_rect(inner_rect, COLOR_WHITE, color, GuiDefaults::MenuItemCornerRadius, MIC_ALL_CORNERS);
}

// * ScreenToolTypeAndColor
ScreenToolTypeAndColor::ScreenToolTypeAndColor(uint8_t tool_ix)
    : ScreenMenu(_("TYPE AND COLOR")) {
    const auto tool = VirtualToolIndex::from_raw(tool_ix);
    Item<screen_type_and_color::MI_FILAMENT_TYPE_SELECT>().set_tool(tool);
    Item<screen_type_and_color::MI_FILAMENT_COLOR_SELECT>().set_tool(tool.to_physical());
}

// * MI_TYPE_AND_COLOR_TOOL
MI_TYPE_AND_COLOR_TOOL::MI_TYPE_AND_COLOR_TOOL(uint8_t tool)
    : IWindowMenuItem({}, nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes)
    , tool_(VirtualToolIndex::from_raw(tool)) {

    StringBuilder sb(label_buffer_);
    sb.append_string_view(_("Tool"));
    sb.append_printf(" %d", tool_.display_index());

    SetLabel(string_view_utf8::MakeRAM(label_buffer_.data()));
    set_is_hidden(!tool_.is_enabled());
}

void MI_TYPE_AND_COLOR_TOOL::click(IWindowMenu &) {
    Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenToolTypeAndColor>(static_cast<uint8_t>(tool_.to_raw())));
}

// * ScreenToolListTypeAndColor
ScreenToolListTypeAndColor::ScreenToolListTypeAndColor()
    : ScreenMenu(_("TYPE AND COLOR")) {}

// * MI_TYPE_AND_COLOR
MI_TYPE_AND_COLOR::MI_TYPE_AND_COLOR()
    : IWindowMenuItem(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no, expands_t::yes) {
    set_is_hidden(!config_store().filament_color_manager_enabled.get());
}

void MI_TYPE_AND_COLOR::click(IWindowMenu &) {
    if (get_num_of_enabled_tools() > 1) {
        Screens::Access()->Open<ScreenToolListTypeAndColor>();
    } else {
        Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenToolTypeAndColor>(uint8_t { 0 }));
    }
}
