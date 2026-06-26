#include "mesh_renderer.h"

#include <segmented_json_macros.h>

#include <cmath>
#include <cstdio>

namespace nhttp::link_content {

using namespace json;

void MeshRenderState::load_cell() {
    const float z = bed_mesh::get_z(x, y);
    if (std::isnan(z)) {
        snprintf(cell.data(), cell.size(), "null");
    } else {
        snprintf(cell.data(), cell.size(), "%.3f", static_cast<double>(z));
    }
}

JsonResult MeshRenderer::renderState(size_t resume_point, JsonOutput &output, MeshRenderState &state) const {
    // clang-format off
    JSON_START;
    JSON_OBJ_START;
        JSON_FIELD_INT("x_points", state.geo.x_points) JSON_COMMA;
        JSON_FIELD_INT("y_points", state.geo.y_points) JSON_COMMA;
        JSON_FIELD_INT("border", state.geo.border) JSON_COMMA;
        JSON_FIELD_INT("major_step", state.geo.major_step) JSON_COMMA;
        JSON_FIELD_INT("x_major", state.geo.x_major) JSON_COMMA;
        JSON_FIELD_INT("y_major", state.geo.y_major) JSON_COMMA;
        JSON_FIELD_FFIXED("x_min", state.geo.x_min, 3) JSON_COMMA;
        JSON_FIELD_FFIXED("y_min", state.geo.y_min, 3) JSON_COMMA;
        JSON_FIELD_FFIXED("x_step", state.geo.x_step, 3) JSON_COMMA;
        JSON_FIELD_FFIXED("y_step", state.geo.y_step, 3) JSON_COMMA;
        JSON_FIELD_BOOL("valid", state.geo.valid) JSON_COMMA;
        JSON_FIELD_ARR("data");
            for (; state.y < state.geo.y_points; state.y++) {
                if (state.y != 0) {
                    JSON_COMMA;
                }
                JSON_CONTROL("[");
                for (; state.x < state.geo.x_points; state.x++) {
                    if (state.x != 0) {
                        JSON_COMMA;
                    }
                    state.load_cell();
                    JSON_CUSTOM("%s", state.cell.data());
                }
                state.x = 0;
                JSON_CONTROL("]");
            }
        JSON_ARR_END;
    JSON_OBJ_END;
    JSON_END;
    // clang-format on
}

} // namespace nhttp::link_content
