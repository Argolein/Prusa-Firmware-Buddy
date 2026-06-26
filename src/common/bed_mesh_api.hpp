#pragma once

#include <cstdint>

/// Read-only access to the current bed mesh (UBL z_values) for non-Marlin tasks
/// (e.g. the network/WUI task that serves the web heatmap viewer).
///
/// The mesh is only written by the Marlin task during G29. Individual aligned
/// 32-bit float reads are atomic on the Cortex-M, so a reader may at worst
/// observe a grid that is partially updated *while* leveling is running — which
/// is cosmetically irrelevant for a viewer. No mesh data is duplicated into
/// marlin_vars: the array cannot live there (MarlinVariable is lock-free/atomic
/// only) and a permanent ~1.8 KB mirror would waste RAM, so access is pull-based.
namespace bed_mesh {

/// Geometry of the mesh grid. Constant for a given printer/configuration.
struct Geometry {
    uint8_t x_points = 0; ///< GRID_MAX_POINTS_X (full, interpolated resolution)
    uint8_t y_points = 0; ///< GRID_MAX_POINTS_Y
    uint8_t border = 0; ///< GRID_BORDER (outer ring that is never probed)
    uint8_t major_step = 1; ///< GRID_MAJOR_STEP (index spacing of probed points)
    uint8_t x_major = 0; ///< GRID_MAJOR_POINTS_X (count of actually-probed points)
    uint8_t y_major = 0; ///< GRID_MAJOR_POINTS_Y
    float x_min = 0; ///< mesh origin X [mm]
    float y_min = 0; ///< mesh origin Y [mm]
    float x_step = 0; ///< distance between adjacent grid points X [mm]
    float y_step = 0; ///< distance between adjacent grid points Y [mm]
    bool valid = false; ///< true if the whole mesh is defined (leveling_is_valid)
};

/// Returns the mesh geometry.
Geometry get_geometry();

/// Returns the probed/interpolated z height [mm] at grid cell (x, y), or NaN if
/// the cell is undefined or out of range. Valid range: x in [0, x_points),
/// y in [0, y_points).
float get_z(uint8_t x, uint8_t y);

} // namespace bed_mesh
