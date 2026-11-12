#pragma once

#include "segmented_json.h"

#include <filament.hpp>
#include <tool_index.hpp>
#include <utils/color.hpp>

#include <array>
#include <cstdint>

namespace nhttp::link_content {

/// Streams the current G-code-filament ↔ printer-tool mapping for
/// GET /api/v1/mapping (Argo: web tool mapping).
///
/// The mapping (indices, types, colors, joins) is snapshotted once at construction
/// — like MeshRenderState snapshots the geometry — so no marlin lock is held while
/// streaming. Names are looked up only while rendering: every connection's handler
/// variant is as large as its largest member, and the network task keeps several
/// of them on its stack, so this state must stay small (text copies here
/// overflowed that stack).
/// When the printer is not in the print-preview "tools mapping" phase, `active`
/// is false and the arrays are empty (the web UI hides the screen).
class ToolMappingRenderState {
public:
    static constexpr uint8_t max_gcode = GcodeToolIndex::count;
    static constexpr uint8_t max_tools = VirtualToolIndex::count;
    static constexpr uint8_t no_palette_color = 0xff;

    /// One used G-code filament (left column) plus its current mapping target.
    /// Its type name is read from GCodeInfo while rendering.
    /// Members ordered by alignment to avoid padding.
    struct GcodeFilament {
        Color color;
        uint8_t index; ///< display index (1-based), as shown to the user
        bool has_color;
        bool has_tool; ///< currently mapped to a printer tool
        uint8_t tool; ///< mapped printer tool display index (1-based)
    };

    /// One printer tool (right column). Disabled slots are shown as placeholders.
    struct Tool {
        Color color;
        FilamentType type; ///< FilamentType::none when unknown or disabled
        uint8_t index; ///< display index (1-based)
        bool enabled;
        bool has_color;
        uint8_t palette_index; ///< into filament_color_palette, or no_palette_color
    };

    /// One spool-join link: when `from` runs out, `to` continues (display indices).
    struct Join {
        uint8_t from;
        uint8_t to;
    };

    bool active = false;
    uint8_t gcode_count = 0;
    uint8_t tool_count = 0;
    uint8_t join_count = 0;
    std::array<GcodeFilament, max_gcode> gcode_filaments {};
    std::array<Tool, max_tools> tools {};
    std::array<Join, max_tools> joins {};

    // Iteration counters, persisted across segmented-render resumes.
    uint8_t gi = 0;
    bool gi_first = true;
    uint8_t ti = 0;
    bool ti_first = true;
    uint8_t ji = 0;
    bool ji_first = true;

    /// Snapshots the live mapping state (phase, filaments, tools, joins).
    ToolMappingRenderState();
};

class ToolMappingRenderer final : public json::JsonRenderer<ToolMappingRenderState> {
public:
    ToolMappingRenderer()
        : JsonRenderer(ToolMappingRenderState()) {}
    json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, ToolMappingRenderState &state) const override;
};

} // namespace nhttp::link_content
