#include "screen_preheat.hpp"

#include "img_resources.hpp"
#include "marlin_client.hpp"
#include "stdlib.h"
#include "i18n.h"
#include <utils/variant_utils.hpp>
#include <filament_gui.hpp>
#include <utils/string_builder.hpp>
#include <gui/screen/filament/screen_filament_detail.hpp>
#include <ScreenHandler.hpp>
#include <gui/standard_frame/frame_prompt.hpp>
#include <utils/color.hpp>
#include <display.hpp>
#include <guiconfig/GuiDefaults.hpp>

#if HAS_ANFC()
    #include <feature/openprinttag/tool_tag.hpp>
    #include <screen/openprinttag/screen_opt_filament_detail.hpp>
    #include <feature/openprinttag/requests_read_multi.hpp>
#endif

namespace {

using PreheatToolIndex = PreheatData::ToolIndex;

class WindowMenuPreheat;

// extra space at the end is intended
class MI_FILAMENT : public WiInfo<sizeof("999/999 ")> {
public:
    MI_FILAMENT(FilamentType filament_type, PreheatToolIndex target_extruder);
    void click(IWindowMenu &) final;

    const FilamentType filament_type;
    const PreheatToolIndex tool;
    FilamentTypeParameters::Name filament_name;
};

#if HAS_ANFC()
class MI_FROM_OPENPRINTTAG : public IWindowMenuItem {
public:
    MI_FROM_OPENPRINTTAG(VirtualToolIndex tool);

    void click(IWindowMenu &) final;
    void Loop() final;

    const VirtualToolIndex tool_;
};

#endif

class WindowMenuPreheat : public WindowMenuVirtual {

public:
    WindowMenuPreheat(window_t *parent, const Rect16 &rect);

    void set_data(const PreheatData &data);
    void set_show_all_filaments(bool set);

    int item_count() const final {
        return index_mapping.total_item_count();
    }

protected:
    void update_list();
    void setup_item(ItemVariant &variant, int index) final;

protected:
    void screenEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    enum class Item {
        return_,
#if HAS_ANFC()
        from_openprinttag,
#endif
        filament_section,
        show_all,
        cooldown,
        adhoc_filament,
    };

    static constexpr auto items = std::to_array<DynamicIndexMappingRecord<Item>>({
        { Item::return_, DynamicIndexMappingType::optional_item },
#if HAS_ANFC()
            { Item::from_openprinttag, DynamicIndexMappingType::optional_item },
#endif
            { Item::filament_section, DynamicIndexMappingType::dynamic_section },
            { Item::adhoc_filament },
            { Item::show_all, DynamicIndexMappingType::optional_item },
            { Item::cooldown, DynamicIndexMappingType::optional_item },
    });

private:
    FilamentList filament_list;
    DynamicIndexMapping<items> index_mapping;
    bool show_all_filaments_ = false;

    /// Extruder we're doing the load/preheat for
    PreheatToolIndex tool = AllTools {};
};

// * MI_FILAMENT
MI_FILAMENT::MI_FILAMENT(FilamentType filament_type, PreheatToolIndex tool)
    : WiInfo({}, nullptr, is_enabled_t::yes, is_hidden_t::no)
    , filament_type(filament_type)
    , tool(tool) //
{
    const auto filament_params = filament_type.parameters();
    filament_name = filament_params.name;

    FilamentTypeGUI::setup_menu_item(filament_type, filament_name, *this);

    ArrayStringBuilder<GetInfoLen()> sb;
    sb.append_printf("%3u/%-3u", filament_params.nozzle_temperature, filament_params.heatbed_temperature);
    ChangeInformation(sb.str());
}

void MI_FILAMENT::click(IWindowMenu &) {
    ScreenPreheat::handle_filament_selection(filament_type, tool);
}

#if HAS_ANFC()
// * MI_FROM_OPENPRINTTAG
MI_FROM_OPENPRINTTAG::MI_FROM_OPENPRINTTAG(VirtualToolIndex tool)
    : IWindowMenuItem(_("Scan OpenPrintTag"))
    , tool_(tool) {
}

void MI_FROM_OPENPRINTTAG::click(IWindowMenu &) {
    const auto tag = buddy::openprinttag::ToolTag::for_tool_ephemeral(tool_);
    if (!tag.has_value()) {
        // Will get disabled in Loop
        return;
    }

    Screens::Access()->Open(buddy::openprinttag::screen_openprinttag_preheat_mode_creator(*tag));
}

void MI_FROM_OPENPRINTTAG::Loop() {
    set_enabled(buddy::openprinttag::ToolTag::for_tool_ephemeral(tool_).has_value());
}
#endif

// * WindowMenuPreheat
WindowMenuPreheat::WindowMenuPreheat(window_t *parent, const Rect16 &rect)
    : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::no) //
{
}

void WindowMenuPreheat::set_data(const PreheatData &data) {
    tool = data.tool;

    index_mapping.set_item_enabled<Item::return_>(data.has_return_option);
    index_mapping.set_item_enabled<Item::cooldown>(data.has_cooldown_option);

#if HAS_ANFC()
    index_mapping.set_item_enabled<Item::from_openprinttag>(
        std::holds_alternative<VirtualToolIndex>(tool) //
        && buddy::openprinttag::has_tool_openprinttag_reader(std::get<VirtualToolIndex>(tool)));
#endif

    update_list();
}

void WindowMenuPreheat::set_show_all_filaments(bool set) {
    if (show_all_filaments_ == set) {
        return;
    }

    const auto prev_focused_index = focused_item_index();
    show_all_filaments_ = set;
    update_list();
    move_focus_to_index(prev_focused_index);
}

void WindowMenuPreheat::update_list() {
    const GenerateFilamentListConfig config {
        .visible_only = !show_all_filaments_,
        .visible_first = true,
    };
    generate_filament_list(filament_list, config);
    index_mapping.set_section_size<Item::filament_section>(filament_list.size());
    index_mapping.set_item_enabled<Item::show_all>(!show_all_filaments_);
    setup_items();

#if HAS_ANFC()
    // If there is an NFC tag detected for the specified tool, auto-focus the "Load from openprinttag"
    if (std::holds_alternative<VirtualToolIndex>(tool) //
        && buddy::openprinttag::ToolTag::for_tool_ephemeral(std::get<VirtualToolIndex>(tool)).has_value()) {
        move_focus_to_index(index_mapping.to_index<Item::from_openprinttag>());
    }
#endif
}

void WindowMenuPreheat::setup_item(ItemVariant &variant, int index) {
    const auto mapping = index_mapping.from_index(index);
    switch (mapping.item) {

    case Item::return_: {
        const auto callback = [this] {
            Validate(); /// don't redraw since we leave the menu
            marlin_client::FSM_response(PhasesPreheat::user_temp_selection, Response::Abort);
        };
        variant.emplace<WindowMenuCallbackItem>(_("Return"), callback, &img::folder_up_16x16);
        break;
    }

#if HAS_ANFC()
    case Item::from_openprinttag:
        variant.emplace<MI_FROM_OPENPRINTTAG>(std::get<VirtualToolIndex>(tool));
        break;
#endif

    case Item::cooldown: {
        const auto callback = [] {
            marlin_client::FSM_response(PhasesPreheat::user_temp_selection, Response::Cooldown);
        };
        variant.emplace<WindowMenuCallbackItem>(_(get_response_text(Response::Cooldown)), callback);
        break;
    }

    case Item::show_all: {
        const auto callback = [this] {
            set_show_all_filaments(true);
        };
        variant.emplace<WindowMenuCallbackItem>(_("Show All"), callback);
        break;
    }

    case Item::filament_section:
        variant.emplace<MI_FILAMENT>(filament_list[mapping.pos_in_section], tool);
        break;

    case Item::adhoc_filament: {
        const auto callback = [this] {
            const ScreenFilamentDetail::PreheatModeParams params {
                .tool = tool,
            };
            Screens::Access()->Open(ScreenFactory::ScreenWithArg<ScreenFilamentDetail>(params));
        };
        variant.emplace<WindowMenuCallbackItem>(_("Custom"), callback);
        break;
    }
    }
}

void WindowMenuPreheat::screenEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {

    case GUI_event_t::TOUCH_SWIPE_LEFT:
    case GUI_event_t::TOUCH_SWIPE_RIGHT:
        if (index_mapping.is_item_enabled<Item::return_>()) {
            marlin_client::FSM_response(PhasesPreheat::user_temp_selection, Response::Abort);
            return;
        }
        break;

    default:
        break;
    }

    WindowMenuVirtual::screenEvent(sender, event, param);
}

// * Frames
using Phase = PhasesPreheat;

struct FrameFilamentSelection {
    WindowExtendedMenu<WindowMenuPreheat> menu;

    FrameFilamentSelection(window_frame_t *parent)
        : menu(parent, parent->GetRect()) {
        parent->CaptureNormalWindow(menu);
    }

    void update(const fsm::PhaseData &data) {
        menu.menu.set_data(PreheatData::deserialize(data));
    }
};
static_assert(common_frames::is_update_callable<FrameFilamentSelection>);

// * Filament Color Manager: color selection during load
class MI_PREHEAT_COLOR : public IWindowMenuItem {
public:
    MI_PREHEAT_COLOR(uint8_t palette_index)
        : IWindowMenuItem(string_view_utf8::MakeCPUFLASH(filament_color_presets[palette_index].name.data()), Rect16::Width_t { 32 })
        , palette_index_(palette_index) {}

    void click(IWindowMenu &) final {
        marlin_client::FSM_response_variant(PhasesPreheat::user_color_selection, FSMResponseVariant::make<Color>(filament_color_presets[palette_index_].color));
    }

protected:
    void printExtension(Rect16 extension_rect, [[maybe_unused]] Color color_text, Color color_back, [[maybe_unused]] ropfn raster_op) const override {
        const Color color = filament_color_presets[palette_index_].color;
        constexpr auto margin = 6;
        constexpr auto padding = 1;

        const auto outer_size = extension_rect.Height() - margin * 2;
        const Rect16 outer_rect = Rect16::fromLTWH(extension_rect.Left(), extension_rect.Top() + margin, outer_size, outer_size);
        display::draw_rounded_rect(outer_rect, color_back, COLOR_WHITE, GuiDefaults::MenuItemCornerRadius, MIC_ALL_CORNERS);

        const auto inner_size = outer_size - padding * 2;
        const Rect16 inner_rect = Rect16::fromLTWH(outer_rect.Left() + padding, outer_rect.Top() + padding, inner_size, inner_size);
        display::draw_rounded_rect(inner_rect, COLOR_WHITE, color, GuiDefaults::MenuItemCornerRadius, MIC_ALL_CORNERS);
    }

private:
    uint8_t palette_index_;
};

class WindowMenuColor : public WindowMenuVirtual {
public:
    WindowMenuColor(window_t *parent, const Rect16 &rect)
        : WindowMenuVirtual(parent, rect, CloseScreenReturnBehavior::no) {
        setup_items();
    }

    int item_count() const final {
        return 1 + static_cast<int>(filament_color_presets.size()); // Return + colors
    }

protected:
    void setup_item(ItemVariant &variant, int index) final {
        if (index == 0) {
            const auto callback = [] {
                marlin_client::FSM_response(PhasesPreheat::user_color_selection, Response::Abort);
            };
            variant.emplace<WindowMenuCallbackItem>(_("Return"), callback, &img::folder_up_16x16);
        } else {
            variant.emplace<MI_PREHEAT_COLOR>(static_cast<uint8_t>(index - 1));
        }
    }
};

struct FrameColorSelection {
    WindowExtendedMenu<WindowMenuColor> menu;

    FrameColorSelection(window_frame_t *parent)
        : menu(parent, parent->GetRect()) {
        parent->CaptureNormalWindow(menu);
    }

    void update(const fsm::PhaseData &) {}
};
static_assert(common_frames::is_update_callable<FrameColorSelection>);

#if HAS_ANFC()
// Note: we need the window_t so that we can hook to the loop event
class FrameAskLoadOpenPrintTag : public FramePrompt {

public:
    FrameAskLoadOpenPrintTag(window_frame_t *parent)
        : FramePrompt(parent, PhasesPreheat::ask_load_openprinttag, _("Load from OpenPrintTag?"), string_view_utf8 {}) {
        update_info_text();
    }

    void update(const fsm::PhaseData &data) {
        const auto d = PreheatData::deserialize(data);

        const auto tool = stdext::get_optional<VirtualToolIndex>(d.tool);
        if (!tool) {
            return;
        }

        const auto tag = buddy::openprinttag::ToolTag::for_tool_ephemeral(*tool);

        if (!tag) {
            return;
        }

        opt_req_.emplace(*tag);
        opt_req_->issue();
        info_updated_ = false;
    }

    void loop() {
        if (!info_updated_ && opt_req_.has_value() && opt_req_->finished()) {
            update_info_text();
            info_updated_ = true;
        }
    }

private:
    void update_info_text() {
        std::string_view brand = "...";
        std::string_view material;

        if (opt_req_.has_value() && opt_req_->finished()) {
            brand = {};

            if (auto r = opt_req_->result<openprinttag::MainField::brand_name>()) {
                brand = *r;
            }
            if (auto r = opt_req_->result<openprinttag::MainField::material_name>()) {
                material = *r;
            }
        }

        info.SetText(_("Load parameters from OpenPrintTag?\n\nMaterial: %.*s %.*s").formatted(text_params_, brand, material));

        // Force invalidation, because the ref is the same
        info.Invalidate();
    }

private:
    std::optional<buddy::openprinttag::MultiReadFieldRequest<openprinttag::MainField::material_name, openprinttag::MainField::brand_name>> opt_req_;
    StringViewUtf8Parameters<64> text_params_;
    bool info_updated_ = false;
};
static_assert(common_frames::is_update_callable<FrameAskLoadOpenPrintTag>);

struct FrameOPTParameters {
    FrameOPTParameters(window_frame_t *) {}

    void update(const fsm::PhaseData &data) {
        const auto d = PreheatData::deserialize(data);

        if (const auto tag = buddy::openprinttag::ToolTag::for_tool_ephemeral(std::get<VirtualToolIndex>(d.tool))) {
            Screens::Access()->Open(screen_openprinttag_preheat_mode_creator(*tag));
        }

        // Switch to a different phase to prevent the screen reopening again after it closes
        // See hack explanation in PhasesPreheat::openprinttag_parameters doxygen
        marlin_client::FSM_response(PhasesPreheat::openprinttag_parameters, Response::Ok);
    }
};
static_assert(common_frames::is_update_callable<FrameOPTParameters>);

#endif

using Frames
    = FrameDefinitionList<ScreenPreheat::FrameStorage,
#if HAS_ANFC()
        FrameDefinition<Phase::ask_load_openprinttag, FrameAskLoadOpenPrintTag>,
        FrameDefinition<Phase::openprinttag_parameters, FrameOPTParameters>,
#endif
        FrameDefinition<Phase::user_temp_selection, FrameFilamentSelection>,
        FrameDefinition<Phase::user_color_selection, FrameColorSelection>>;

} // namespace

// * ScreenPreheat
ScreenPreheat::ScreenPreheat()
    : ScreenFSM(nullptr, GuiDefaults::RectScreenNoHeader) {
    create_frame();
}

ScreenPreheat::~ScreenPreheat() {
    destroy_frame();
}

bool ScreenPreheat::handle_filament_selection(FilamentType filament_type, PreheatData::ToolIndex tool) {
    const auto filament = filament_type.parameters();

    if (filament.is_abrasive && std::holds_alternative<VirtualToolIndex>(tool) && !config_store().get_nozzle_is_hardened(std::get<VirtualToolIndex>(tool).to_physical())) {
        StringViewUtf8Parameters<filament_name_buffer_size + 1> params;
        if (MsgBoxWarning(_("Filament '%s' is abrasive, but you don't have a hardened nozzle installed. Do you really want to continue?").formatted(params, filament.name.data()), Responses_YesNo) != Response::Yes) {
            return false;
        }
    }

    marlin_client::FSM_response_variant(PhasesPreheat::user_temp_selection, FSMResponseVariant::make<FilamentType>(filament_type));
    return true;
}

void ScreenPreheat::screenEvent(window_t *sender, GUI_event_t event, void *param) {
#if HAS_ANFC()
    if (event == GUI_event_t::LOOP && get_phase() == PhasesPreheat::ask_load_openprinttag) {
        frame_storage.as<FrameAskLoadOpenPrintTag>()->loop();
    }
#endif

    ScreenFSM::screenEvent(sender, event, param);
}

void ScreenPreheat::create_frame() {
    Frames::create_frame(frame_storage, get_phase(), &inner_frame);
}

void ScreenPreheat::destroy_frame() {
    Frames::destroy_frame(frame_storage, get_phase());
}

void ScreenPreheat::update_frame() {
    const PreheatData data = PreheatData::deserialize(fsm_base_data.GetData());

    Frames::update_frame(frame_storage, get_phase(), fsm_base_data.GetData());

    const auto title = [&] -> const char * {
        switch (data.mode) {
        case PreheatMode::preheat:
            return N_("Preheating");

        case PreheatMode::standard_load:
        case PreheatMode::change_load:
        case PreheatMode::autoload:
            return N_("Preheating for load");

        case PreheatMode::unload:
            return N_("Preheating for unload");

        case PreheatMode::purge:
            return N_("Preheating for purge");
        }
        bsod_unreachable();
    }();

    header.SetText(_(title));
}
