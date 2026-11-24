#include <handler.h>

// The real implementations reach into marlin_vars, the filament store and the
// tool mapper; the handler variant only needs them to link.
namespace nhttp::link_content {

json::JsonResult FilamentRenderer::renderState(size_t, json::JsonOutput &, FilamentRenderState &) const {
    return json::JsonResult::Complete;
}

json::JsonResult ToolMappingRenderer::renderState(size_t, json::JsonOutput &, ToolMappingRenderState &) const {
    return json::JsonResult::Complete;
}

} // namespace nhttp::link_content

namespace nhttp::printer {

void FilamentCommand::step(std::string_view, bool, uint8_t *, size_t, handler::Step &) {}

void ToolMappingCommand::step(std::string_view, bool, uint8_t *, size_t, handler::Step &) {}

} // namespace nhttp::printer
