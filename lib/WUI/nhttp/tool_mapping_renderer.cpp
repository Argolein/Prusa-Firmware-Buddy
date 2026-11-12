#include "tool_mapping_renderer.h"
#include "filament_renderer.h"

#include <segmented_json_macros.h>

#include <option/has_tool_mapping.h>
#include <option/has_spool_join.h>

#if HAS_TOOL_MAPPING()
    #include <config_store/store_instance.hpp>
    #include <utils/color.hpp>
    #include <utils/variant_utils.hpp>
    #include <gcode/gcode_info.hpp>
    #include <marlin_vars.hpp>
    #include <fsm_states.hpp>
    #include <client_response.hpp>
    #include <fsm/print_preview_phases.hpp>
    #include <module/prusa/tool_mapper.hpp>
    #if HAS_SPOOL_JOIN()
        #include <module/prusa/spool_join.hpp>
    #endif
#endif

#include <cstdio>

namespace nhttp::link_content {

using namespace json;

ToolMappingRenderState::ToolMappingRenderState() {
#if HAS_TOOL_MAPPING()
    // Only expose the mapping while the print-preview FSM is actually holding at
    // the tools-mapping phase (the same screen the LCD shows). Otherwise report
    // inactive and leave the arrays empty.
    std::optional<fsm::States::Top> top;
    marlin_vars().peek_fsm_states([&](const fsm::States &states) {
        top = states.get_top();
    });
    active = top.has_value()
        && top->fsm_type == ClientFSM::PrintPreview
        && GetEnumFromPhaseIndex<PhasesPrintPreview>(top->data.GetPhase()) == PhasesPrintPreview::tools_mapping;

    if (!active) {
        return;
    }

    const auto &gci = GCodeInfo::getInstance();

    // Left column: G-code filaments actually used by the file, with their current
    // mapping target (gcode tool -> virtual/printer tool).
    for (const auto gcode_tool : GcodeToolIndex::all()) {
        const auto &ei = gci.get_extruder_info(gcode_tool);
        if (!ei.used()) {
            continue;
        }
        GcodeFilament &f = gcode_filaments[gcode_count++];
        f.index = gcode_tool.display_index();

        f.has_color = ei.extruder_colour.has_value();
        if (f.has_color) {
            f.color = *ei.extruder_colour;
        }

        const auto virtual_tool = stdext::get_optional<VirtualToolIndex>(tool_mapper.to_virtual(gcode_tool));
        f.has_tool = virtual_tool.has_value();
        if (f.has_tool) {
            f.tool = virtual_tool->display_index();
        }
    }

    // Right column: printer tools across the enabled range, so an empty slot in
    // the middle (e.g. MMU slot 4) is shown as a placeholder like on the LCD.
    // Type and color come from the tool's loaded filament.
    const uint8_t range = VirtualToolIndex::enabled_range_size();
    for (uint8_t raw = 0; raw < range; raw++) {
        const auto virtual_tool = VirtualToolIndex::from_raw(raw);
        Tool &t = tools[tool_count++];
        t.index = virtual_tool.display_index();
        t.enabled = virtual_tool.is_enabled();
        t.type = FilamentType::none;
        t.has_color = false;
        t.palette_index = no_palette_color;

        // Only resolve type/color for enabled tools — a disabled placeholder slot
        // (e.g. an empty MMU slot in the middle of the range) has no physical tool
        // to convert to, and is shown as "---" like on the LCD.
        if (!t.enabled) {
            continue;
        }

        t.type = config_store().get_filament_type(virtual_tool);
        if (t.type == FilamentType::none) {
            continue;
        }

        const FilamentTypeParameters params = t.type.parameters();
        t.has_color = params.color.has_value();
        if (t.has_color) {
            t.color = *params.color;
            for (uint8_t p = 0; p < filament_color_palette.size(); p++) {
                if (filament_color_palette[p].color == t.color) {
                    t.palette_index = p;
                    break;
                }
            }
        }
    }

    #if HAS_SPOOL_JOIN()
    const uint8_t num_joins = spool_join.get_num_joins();
    for (uint8_t i = 0; i < num_joins && join_count < joins.size(); i++) {
        const auto jc = spool_join.get_join_nr(i);
        joins[join_count++] = Join {
            .from = static_cast<uint8_t>(jc.spool_1 + 1),
            .to = static_cast<uint8_t>(jc.spool_2 + 1),
        };
    }
    #endif
#endif // HAS_TOOL_MAPPING()
}

namespace {

    /// Type name of a G-code filament, or nullptr when the file names none.
    /// GCodeInfo holds the previewed file for the whole tools-mapping phase.
    const char *gcode_filament_type([[maybe_unused]] uint8_t display_index) {
#if HAS_TOOL_MAPPING()
        const auto &name = GCodeInfo::getInstance().get_extruder_info(GcodeToolIndex::from_raw(display_index - 1)).filament_name;
        return name.size() > 0 ? name.data() : nullptr;
#else
        return nullptr;
#endif
    }

} // namespace

JsonResult ToolMappingRenderer::renderState(size_t resume_point, JsonOutput &output, ToolMappingRenderState &state) const {
    // clang-format off
    JSON_START;
    JSON_OBJ_START;
        JSON_FIELD_BOOL("active", state.active) JSON_COMMA;

        // NOTE: no local references to the current element here — the JSON_* macros
        // expand to `case` labels in a resumable switch, so a local declared inside
        // the loop body would be "crossed" by a case label (jump-to-case-label error).
        // Access state.<array>[state.<idx>] directly, like FilamentRenderer does.
        JSON_FIELD_ARR("gcode_filaments");
        for (; state.gi < state.gcode_count; state.gi++) {
            if (!state.gi_first) {
                JSON_COMMA;
            }
            state.gi_first = false;

            JSON_OBJ_START;
                JSON_FIELD_INT("index", state.gcode_filaments[state.gi].index) JSON_COMMA;
                if (gcode_filament_type(state.gcode_filaments[state.gi].index)) {
                    // Re-evaluated on resume; the next file scan may have reset GCodeInfo since.
                    JSON_FIELD_STR("type", gcode_filament_type(state.gcode_filaments[state.gi].index) ?: "");
                } else {
                    JSON_CONTROL("\"type\":null");
                }
                JSON_COMMA;
                if (state.gcode_filaments[state.gi].has_color) {
                    JSON_FIELD_STR_FORMAT("color_rgb", "#%02x%02x%02x", state.gcode_filaments[state.gi].color.r, state.gcode_filaments[state.gi].color.g, state.gcode_filaments[state.gi].color.b);
                } else {
                    JSON_CONTROL("\"color_rgb\":null");
                }
                JSON_COMMA;
                if (state.gcode_filaments[state.gi].has_tool) {
                    JSON_FIELD_INT("tool", state.gcode_filaments[state.gi].tool);
                } else {
                    JSON_CONTROL("\"tool\":null");
                }
            JSON_OBJ_END;
        }
        JSON_ARR_END;
        JSON_COMMA;

        JSON_FIELD_ARR("tools");
        for (; state.ti < state.tool_count; state.ti++) {
            if (!state.ti_first) {
                JSON_COMMA;
            }
            state.ti_first = false;

            JSON_OBJ_START;
                JSON_FIELD_INT("index", state.tools[state.ti].index) JSON_COMMA;
                JSON_FIELD_BOOL("enabled", state.tools[state.ti].enabled) JSON_COMMA;
                if (state.tools[state.ti].type != FilamentType::none) {
                    JSON_FIELD_STR("type", state.tools[state.ti].type.parameters().name.data());
                } else {
                    JSON_CONTROL("\"type\":null");
                }
                JSON_COMMA;
                if (state.tools[state.ti].has_color) {
                    JSON_FIELD_STR_FORMAT("color_rgb", "#%02x%02x%02x", state.tools[state.ti].color.r, state.tools[state.ti].color.g, state.tools[state.ti].color.b);
                } else {
                    JSON_CONTROL("\"color_rgb\":null");
                }
                JSON_COMMA;
                if (state.tools[state.ti].palette_index != ToolMappingRenderState::no_palette_color) {
                    JSON_FIELD_STR("color_name", filament_color_palette[state.tools[state.ti].palette_index].name.data());
                } else {
                    JSON_CONTROL("\"color_name\":null");
                }
            JSON_OBJ_END;
        }
        JSON_ARR_END;
        JSON_COMMA;

        JSON_FIELD_ARR("spool_join");
        for (; state.ji < state.join_count; state.ji++) {
            if (!state.ji_first) {
                JSON_COMMA;
            }
            state.ji_first = false;

            JSON_OBJ_START;
                JSON_FIELD_INT("from", state.joins[state.ji].from) JSON_COMMA;
                JSON_FIELD_INT("to", state.joins[state.ji].to);
            JSON_OBJ_END;
        }
        JSON_ARR_END;
    JSON_OBJ_END;
    JSON_END;
    // clang-format on
}

} // namespace nhttp::link_content
