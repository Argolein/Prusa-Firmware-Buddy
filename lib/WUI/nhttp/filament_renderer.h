#pragma once

#include "segmented_json.h"

#include <array>
#include <cstdint>

namespace nhttp::link_content {

/// Per-tool filament type + color rendered for GET /api/v1/filament.
/// Iteration state persists across resumes of the segmented renderer.
class FilamentRenderState {
public:
    uint8_t tool = 0;
    bool first = true;

    // Filled by load() at the start of each tool object; persisted across resumes.
    bool has_type = false;
    bool has_color = false;
    std::array<char, 16> type_name {};
    std::array<char, 16> color_name {};
    std::array<char, 8> color_rgb {};

    /// Loads the current tool's type + color into the buffers above.
    void load();
};

class FilamentRenderer final : public json::JsonRenderer<FilamentRenderState> {
public:
    FilamentRenderer()
        : JsonRenderer(FilamentRenderState()) {}
    json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, FilamentRenderState &state) const override;
};

} // namespace nhttp::link_content
