#include <bed_mesh_api.hpp>

#include <cmath>

// Deterministic stand-in for the real bed_mesh accessor (which pulls in Marlin).
// 3x3 grid; the last cell is undefined (NaN) to exercise the JSON `null` path.
namespace bed_mesh {

Geometry get_geometry() {
    Geometry g;
    g.x_points = 3;
    g.y_points = 3;
    g.border = 1;
    g.major_step = 1;
    g.x_major = 3;
    g.y_major = 3;
    g.x_min = 0.0f;
    g.y_min = 0.0f;
    g.x_step = 10.0f;
    g.y_step = 10.0f;
    g.valid = true;
    return g;
}

float get_z(uint8_t x, uint8_t y) {
    if (x == 2 && y == 2) {
        return NAN;
    }
    return static_cast<float>(x + y * 3) * 0.01f;
}

} // namespace bed_mesh
