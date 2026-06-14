#include "mesh_renderer.hpp"
#include <marlin_vars.hpp>
#include <segmented_json_macros.h>
#include <cmath>

namespace nhttp::link_content {

json::JsonResult MeshRenderer::renderState(size_t resume_point, json::JsonOutput &output, MeshState &state) const {
    // Take a single consistent snapshot of the mesh on first entry. The renderer
    // may be resumed multiple times across chunks, but the state survives so we
    // only read the locked snapshot once per response.
    if (!state.snapshot_taken) {
#if HAS_MESH
        state.mesh = marlin_vars().mesh_data.get();
        state.valid = state.mesh.valid;
#else
        state.valid = false;
#endif
        state.snapshot_taken = true;
    }

    JSON_START;
    JSON_OBJ_START;
        JSON_FIELD_BOOL("valid", state.valid) JSON_COMMA;
#if HAS_MESH
        JSON_FIELD_FFIXED("x_min", state.mesh.x_min, 2) JSON_COMMA;
        JSON_FIELD_FFIXED("y_min", state.mesh.y_min, 2) JSON_COMMA;
        JSON_FIELD_FFIXED("x_dist", state.mesh.x_dist, 4) JSON_COMMA;
        JSON_FIELD_FFIXED("y_dist", state.mesh.y_dist, 4) JSON_COMMA;
        JSON_FIELD_INT("points_x", state.mesh.points_x) JSON_COMMA;
        JSON_FIELD_INT("points_y", state.mesh.points_y) JSON_COMMA;
        JSON_FIELD_ARR("mesh");
            for (state.i = 0; state.i < state.mesh.points_x; state.i++) {
                JSON_CONTROL("[");
                for (state.j = 0; state.j < state.mesh.points_y; state.j++) {
                    if (std::isnan(state.mesh.z_values[state.i][state.j])) {
                        JSON_CONTROL("null");
                    } else {
                        JSON_CUSTOM("%.4f", static_cast<double>(state.mesh.z_values[state.i][state.j]));
                    }
                    if (state.j < state.mesh.points_y - 1) JSON_COMMA;
                }
                JSON_ARR_END;
                if (state.i < state.mesh.points_x - 1) JSON_COMMA;
            }
        JSON_ARR_END;
#endif
    JSON_OBJ_END;
    JSON_END;
}

} // namespace nhttp::link_content
