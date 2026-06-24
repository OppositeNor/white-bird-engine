#include "function/render_software/renderer_software.hh"
#include "core/allocator/i_allocator.hh"

namespace WhiteBirdEngine {

RendererSoftware::RendererSoftware() {
    // TODO
}
RendererSoftware::~RendererSoftware() {
    // TODO
}

void RendererSoftware::tick(double p_delta) {
    // TODO
}
Ref<RenderDatabase> RendererSoftware::get_render_database() {
    return MEM_NULL; // TODO
}
void RendererSoftware::activate_render_graph(const std::string& p_pipeline_name) {
    // TODO
}
void RendererSoftware::update_static_opaque_triangles() {
    // TODO
}
void RendererSoftware::add_job(std::function<void()> p_job) {
    // TODO
}
} // namespace WhiteBirdEngine
