#pragma once
#include "../nhttp/segmented_json.h"
#include <marlin_vars.hpp>

namespace nhttp::link_content {

struct MeshState {
    uint8_t i = 0;
    uint8_t j = 0;
    bool snapshot_taken = false;
    bool valid = false;
#if HAS_MESH
    marlin_vars_t::MeshData mesh {};
#endif
};

class MeshRenderer final : public json::JsonRenderer<MeshState> {
public:
    MeshRenderer() : JsonRenderer(MeshState()) {}
    virtual json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, MeshState &state) const override;
};

} // namespace nhttp::link_content
