#include <mesh_renderer.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using json::JsonResult;
using nhttp::link_content::MeshRenderer;
using std::string;
using std::string_view;

namespace {

// Matches the 3x3 grid from mesh_mock.cpp; last cell is undefined -> null.
const constexpr char *const EXPECTED
    = "{\"x_points\":3,\"y_points\":3,\"border\":1,\"major_step\":1,"
      "\"x_major\":3,\"y_major\":3,"
      "\"x_min\":0.000,\"y_min\":0.000,\"x_step\":10.000,\"y_step\":10.000,"
      "\"valid\":true,"
      "\"data\":[[0.000,0.010,0.020],[0.030,0.040,0.050],[0.060,0.070,null]]}";

} // namespace

TEST_CASE("Mesh renderer - whole thing in one buffer") {
    MeshRenderer renderer;

    constexpr size_t BUF_SIZE = 1024;
    uint8_t buffer[BUF_SIZE];

    const auto [result, written] = renderer.render(buffer, BUF_SIZE);

    REQUIRE(result == JsonResult::Complete);
    REQUIRE(string_view(reinterpret_cast<char *>(buffer), written) == EXPECTED);
}

TEST_CASE("Mesh renderer - split across small buffers") {
    MeshRenderer renderer;

    size_t increment = 24;
    SECTION("tiny") { increment = 24; }
    SECTION("mid") { increment = 64; }
    SECTION("large") { increment = 200; }

    string response;
    auto result = JsonResult::Incomplete;

    while (result != JsonResult::Complete) {
        uint8_t buffer[increment];
        const auto [result_partial, written] = renderer.render(buffer, increment);
        REQUIRE(written <= increment);
        REQUIRE(written > 0);
        response += string_view(reinterpret_cast<char *>(buffer), written);
        result = result_partial;
    }

    REQUIRE(response == EXPECTED);
}
