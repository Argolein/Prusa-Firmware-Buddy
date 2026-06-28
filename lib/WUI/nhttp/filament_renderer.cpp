#include "filament_renderer.h"

#include <segmented_json_macros.h>
#include <filament.hpp>
#include <tool_index.hpp>

#include <cstdio>

namespace nhttp::link_content {

using namespace json;

void FilamentRenderState::load() {
    const auto virtual_tool = VirtualToolIndex::from_raw(tool);
    // A disabled slot inside the range (e.g. an empty MMU slot) is listed as empty.
    const FilamentType type = virtual_tool.is_enabled() ? FilamentType::for_tool(virtual_tool) : FilamentType::none;
    has_type = (type != FilamentType::none);
    has_color = false;
    color_name[0] = '\0';
    if (!has_type) {
        return;
    }

    const FilamentTypeParameters params = type.parameters();
    snprintf(type_name.data(), type_name.size(), "%s", params.name.data());

    has_color = params.color.has_value();
    if (!has_color) {
        return;
    }
    const Color color = *params.color;
    snprintf(color_rgb.data(), color_rgb.size(), "#%02x%02x%02x", color.r, color.g, color.b);
    for (const auto &preset : filament_color_palette) {
        if (preset.color == color) {
            snprintf(color_name.data(), color_name.size(), "%.*s", static_cast<int>(preset.name.size()), preset.name.data());
            break;
        }
    }
}

void FilamentRenderState::load_palette() {
    const auto &preset = filament_color_palette[palette];
    snprintf(palette_name.data(), palette_name.size(), "%.*s", static_cast<int>(preset.name.size()), preset.name.data());
    snprintf(palette_rgb.data(), palette_rgb.size(), "#%02x%02x%02x", preset.color.r, preset.color.g, preset.color.b);
}

JsonResult FilamentRenderer::renderState(size_t resume_point, JsonOutput &output, FilamentRenderState &state) const {
    // clang-format off
    JSON_START;
    JSON_OBJ_START;
        JSON_FIELD_ARR("tools");
        for (; state.tool < VirtualToolIndex::enabled_range_size(); state.tool++) {
            state.load();

            if (!state.first) {
                JSON_COMMA;
            }
            state.first = false;

            JSON_OBJ_START;
                JSON_FIELD_INT("tool", state.tool) JSON_COMMA;

                if (state.has_type) {
                    JSON_FIELD_STR("type", state.type_name.data());
                } else {
                    JSON_CONTROL("\"type\":null");
                }
                JSON_COMMA;

                if (state.color_name[0] != '\0') {
                    JSON_FIELD_STR("color", state.color_name.data());
                } else {
                    JSON_CONTROL("\"color\":null");
                }
                JSON_COMMA;

                if (state.has_color) {
                    JSON_FIELD_STR("color_rgb", state.color_rgb.data());
                } else {
                    JSON_CONTROL("\"color_rgb\":null");
                }
            JSON_OBJ_END;
        }
        JSON_ARR_END;
        JSON_COMMA;
        // Selectable color palette (single source of truth for the web picker).
        JSON_FIELD_ARR("palette");
        for (; state.palette < filament_color_palette.size(); state.palette++) {
            state.load_palette();

            if (!state.palette_first) {
                JSON_COMMA;
            }
            state.palette_first = false;

            JSON_OBJ_START;
                JSON_FIELD_STR("name", state.palette_name.data()) JSON_COMMA;
                JSON_FIELD_STR("color_rgb", state.palette_rgb.data());
            JSON_OBJ_END;
        }
        JSON_ARR_END;
    JSON_OBJ_END;
    JSON_END;
    // clang-format on
}

} // namespace nhttp::link_content
