/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "function/render_software/renderer_software.hh"
#include "core/allocator/i_allocator.hh"

namespace WhiteBirdEngine {

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
