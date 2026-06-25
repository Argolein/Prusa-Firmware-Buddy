#include "filament_renderer.h"

#include <segmented_json_macros.h>
#include <config_store/store_instance.hpp>
#include <utils/color.hpp>
#include <tool_index.hpp>

#include <cstdio>

namespace nhttp::link_content {

using namespace json;

void FilamentRenderState::load() {
    const FilamentType type = FilamentType::for_tool(VirtualToolIndex::from_raw(tool));
    has_type = (type != FilamentType::none);
    if (has_type) {
        const auto name = type.parameters().name;
        snprintf(type_name.data(), type_name.size(), "%s", name.data());
    }

    const auto color_index = config_store().get_filament_color(PhysicalToolIndex::from_raw(tool));
    has_color = color_index.has_value();
    if (has_color) {
        const auto &preset = filament_color_presets[*color_index];
        snprintf(color_name.data(), color_name.size(), "%.*s", static_cast<int>(preset.name.size()), preset.name.data());
        snprintf(color_rgb.data(), color_rgb.size(), "#%02x%02x%02x", preset.color.r, preset.color.g, preset.color.b);
    }
}

JsonResult FilamentRenderer::renderState(size_t resume_point, JsonOutput &output, FilamentRenderState &state) const {
    // clang-format off
    JSON_START;
    JSON_OBJ_START;
        JSON_FIELD_ARR("tools");
        for (; state.tool < PhysicalToolIndex::count; state.tool++) {
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

                if (state.has_color) {
                    JSON_FIELD_STR("color", state.color_name.data()) JSON_COMMA;
                    JSON_FIELD_STR("color_rgb", state.color_rgb.data());
                } else {
                    JSON_CONTROL("\"color\":null,\"color_rgb\":null");
                }
            JSON_OBJ_END;
        }
        JSON_ARR_END;
    JSON_OBJ_END;
    JSON_END;
    // clang-format on
}

} // namespace nhttp::link_content
