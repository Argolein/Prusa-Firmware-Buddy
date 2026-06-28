#pragma once

#include "segmented_json.h"

#include <utils/color555.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nhttp::link_content {

struct FilamentColorPreset {
    std::string_view name;
    Color color;
};

/// Named colors offered by PrusaLink. Same entries as the printer's color menu
/// (src/gui/menu_item/menu_item_color.cpp, minus "None"); keep the two in sync.
/// Colors pass through Color555 like the config store does, so a stored palette
/// color compares equal to its entry here.
inline constexpr std::array filament_color_palette {
    FilamentColorPreset { "Black", Color555::from_rgb(0, 0, 0).to_color() },
    FilamentColorPreset { "White", Color555::from_rgb(255, 255, 255).to_color() },
    FilamentColorPreset { "Gray", Color555::from_rgb(128, 128, 128).to_color() },
    FilamentColorPreset { "Red", Color555::from_rgb(255, 0, 0).to_color() },
    FilamentColorPreset { "Orange", Color555::from_rgb(255, 165, 0).to_color() },
    FilamentColorPreset { "Yellow", Color555::from_rgb(255, 255, 0).to_color() },
    FilamentColorPreset { "Green", Color555::from_rgb(0, 255, 0).to_color() },
    FilamentColorPreset { "Blue", Color555::from_rgb(0, 0, 255).to_color() },
    FilamentColorPreset { "Purple", Color555::from_rgb(128, 0, 128).to_color() },
    FilamentColorPreset { "Pink", Color555::from_rgb(255, 192, 203).to_color() },
    FilamentColorPreset { "Brown", Color555::from_rgb(165, 42, 42).to_color() },
    FilamentColorPreset { "Beige", Color555::from_rgb(245, 245, 220).to_color() },
    FilamentColorPreset { "Turquoise", Color555::from_rgb(64, 224, 208).to_color() },
    FilamentColorPreset { "Gold", Color555::from_rgb(255, 215, 0).to_color() },
    FilamentColorPreset { "Silver", Color555::from_rgb(192, 192, 192).to_color() },
};

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
    /// Palette name of the color, empty when the color is not a palette entry.
    std::array<char, 16> color_name {};
    std::array<char, 8> color_rgb {};

    // Palette iteration (the selectable colors), rendered after "tools" so the web
    // color picker can use the printer's own list as the single source of truth.
    uint8_t palette = 0;
    bool palette_first = true;
    std::array<char, 16> palette_name {};
    std::array<char, 8> palette_rgb {};

    /// Loads the current tool's type + color into the buffers above.
    void load();

    /// Loads the current palette entry's name + rgb into the buffers above.
    void load_palette();
};

class FilamentRenderer final : public json::JsonRenderer<FilamentRenderState> {
public:
    FilamentRenderer()
        : JsonRenderer(FilamentRenderState()) {}
    json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, FilamentRenderState &state) const override;
};

} // namespace nhttp::link_content
