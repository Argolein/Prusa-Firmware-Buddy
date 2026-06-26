#include "bed_mesh_api.hpp"

#include <cmath>

#include "../lib/Marlin/Marlin/src/feature/bedlevel/bedlevel.h"
#if ENABLED(AUTO_BED_LEVELING_UBL)
    #include "../lib/Marlin/Marlin/src/feature/bedlevel/ubl/ubl.h"
#endif

namespace bed_mesh {

Geometry get_geometry() {
    Geometry g;
#if ENABLED(AUTO_BED_LEVELING_UBL)
    g.x_points = GRID_MAX_POINTS_X;
    g.y_points = GRID_MAX_POINTS_Y;
    #ifdef GRID_BORDER
    g.border = GRID_BORDER;
    #endif
    #ifdef GRID_MAJOR_STEP
    g.major_step = GRID_MAJOR_STEP;
    #endif
    #ifdef GRID_MAJOR_POINTS_X
    g.x_major = GRID_MAJOR_POINTS_X;
    g.y_major = GRID_MAJOR_POINTS_Y;
    #endif
    g.x_min = MESH_MIN_X;
    g.y_min = MESH_MIN_Y;
    g.x_step = MESH_X_DIST;
    g.y_step = MESH_Y_DIST;
    g.valid = leveling_is_valid();
#endif
    return g;
}

float get_z([[maybe_unused]] uint8_t x, [[maybe_unused]] uint8_t y) {
#if ENABLED(AUTO_BED_LEVELING_UBL)
    if (x < GRID_MAX_POINTS_X && y < GRID_MAX_POINTS_Y) {
        return unified_bed_leveling::z_values[x][y];
    }
#endif
    return NAN;
}

} // namespace bed_mesh
