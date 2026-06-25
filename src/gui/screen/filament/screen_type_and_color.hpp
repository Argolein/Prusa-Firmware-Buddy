#pragma once

#include <array>
#include <utility>

#include <i_window_menu_item.hpp>
#include <WindowMenuItems.hpp>
#include <screen_menu.hpp>
#include <tool_index.hpp>
#include <filament_list.hpp>
#include <gui/menu_item/menu_item_select_menu.hpp>

namespace screen_type_and_color {

/// Per-tool filament type selection.
/// The "type" is the tool's real loaded filament type, so editing it updates
/// config_store().loaded_filament_type via set_filament_type().
class MI_FILAMENT_TYPE_SELECT : public MenuItemSelectMenu {
public:
    MI_FILAMENT_TYPE_SELECT();

    /// Binds the item to a tool and refreshes the shown value.
    void set_tool(VirtualToolIndex tool);

    int item_count() const final;
    string_view_utf8 build_item_text(int index, ItemTextParams &params) const final;

protected:
    bool on_item_selected(const OnItemSelectedArgs &args) override;

private:
    void rebuild();

    VirtualToolIndex tool_ = VirtualToolIndex::from_raw(0);
    FilamentList filament_list_;
};

/// Per-tool filament color selection (cosmetic metadata).
/// Renders the currently selected color as a swatch instead of text.
class MI_FILAMENT_COLOR_SELECT : public MenuItemSelectMenu {
public:
    MI_FILAMENT_COLOR_SELECT();

    /// Binds the item to a tool and refreshes the shown value.
    void set_tool(PhysicalToolIndex tool);

    int item_count() const final;
    string_view_utf8 build_item_text(int index, ItemTextParams &params) const final;

protected:
    bool on_item_selected(const OnItemSelectedArgs &args) override;
    void printExtension(Rect16 extension_rect, Color color_text, Color color_back, ropfn raster_op) const override;

private:
    PhysicalToolIndex tool_ = PhysicalToolIndex::from_raw(0);
};

} // namespace screen_type_and_color

/// Per-tool "Type and color" screen.
class ScreenToolTypeAndColor : public ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN,
                                   screen_type_and_color::MI_FILAMENT_TYPE_SELECT,
                                   screen_type_and_color::MI_FILAMENT_COLOR_SELECT> {
public:
    ScreenToolTypeAndColor(uint8_t tool_ix = 0);
};

/// One entry per tool in the multitool list; opens ScreenToolTypeAndColor for that tool.
class MI_TYPE_AND_COLOR_TOOL : public IWindowMenuItem {
public:
    MI_TYPE_AND_COLOR_TOOL(uint8_t tool = 0);

protected:
    void click(IWindowMenu &) override;

private:
    std::array<char, 24> label_buffer_;
    VirtualToolIndex tool_;
};

template <typename>
struct ScreenToolListTypeAndColor_ {};

template <size_t... i>
struct ScreenToolListTypeAndColor_<std::index_sequence<i...>> {
    using T = ScreenMenu<GuiDefaults::MenuFooter, MI_RETURN, WithConstructorArgs<MI_TYPE_AND_COLOR_TOOL, i>...>;
};

/// Tool list screen used on multitool printers (INDX).
class ScreenToolListTypeAndColor : public ScreenToolListTypeAndColor_<std::make_index_sequence<VirtualToolIndex::count>>::T {
public:
    ScreenToolListTypeAndColor();
};

/// Entry item placed in the Filament menu. Hidden when the feature is disabled.
/// Opens the tool list on multitool printers, or the single-tool screen directly.
class MI_TYPE_AND_COLOR : public IWindowMenuItem {
    static constexpr const char *const label = N_("Type and Color");

public:
    MI_TYPE_AND_COLOR();

protected:
    void click(IWindowMenu &) override;
};
