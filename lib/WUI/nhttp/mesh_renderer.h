#pragma once

#include "segmented_json.h"

#include <bed_mesh_api.hpp>

#include <array>
#include <cstdint>

namespace nhttp::link_content {

/// Streams the current bed mesh for GET /api/v1/mesh.
///
/// The grid is rendered cell-by-cell. Only the current indices plus one
/// formatted cell are kept across resumes, so the connection state stays tiny
/// (no full-grid snapshot bloating every connection slot). Undefined cells are
/// emitted as JSON `null`. Geometry is snapshotted once at construction.
class MeshRenderState {
public:
    bed_mesh::Geometry geo = bed_mesh::get_geometry();
    uint8_t x = 0; ///< current column, persisted across resumes
    uint8_t y = 0; ///< current row, persisted across resumes
    std::array<char, 16> cell {}; ///< formatted current cell ("null" or a number)

    /// Formats z at (x, y) into `cell`.
    void load_cell();
};

class MeshRenderer final : public json::JsonRenderer<MeshRenderState> {
public:
    MeshRenderer()
        : JsonRenderer(MeshRenderState()) {}
    json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, MeshRenderState &state) const override;
};

} // namespace nhttp::link_content
