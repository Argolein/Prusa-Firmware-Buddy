#pragma once
#include <segmented_json.h>
#include "inc/MarlinConfig.h"

#if HAS_MESH
    #include <feature/bedlevel/bedlevel.h>
#endif

namespace nhttp::link_content {

struct MeshSnapshot {
    bool valid = false;
#if HAS_MESH
    float z_values[GRID_MAX_POINTS_X][GRID_MAX_POINTS_Y] {};
    float x_min = 0;
    float y_min = 0;
    float x_dist = 0;
    float y_dist = 0;
    uint8_t points_x = 0;
    uint8_t points_y = 0;
#endif
};

struct MeshState {
    uint8_t i = 0;
    uint8_t j = 0;
    bool snapshot_taken = false;
    MeshSnapshot mesh {};
};

class MeshRenderer final : public json::JsonRenderer<MeshState> {
public:
    MeshRenderer() : JsonRenderer(MeshState()) {}
    virtual json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, MeshState &state) const override;
};

} // namespace nhttp::link_content
