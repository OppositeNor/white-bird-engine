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
#include "function/rend_software/renderer_software.hh"
#include "core/alloc/i_alloc.hh"
#include "core/job/job_handler.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_rend_cache.hh"
#include "function/render/i_rend_graph.hh"
#include "function/render/i_rend_graph_bindings.hh"
#include "function/render/i_renderer.hh"
#include "function/render/rend_defs.hh"
#include <cstddef>
#include <format>
#include <functional>
#include <future>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

RendererSoftware::RendererSoftware(Ref<RendAlloc> p_alloc, RendererCapabilities& p_capabilities)
    : rend_alloc(p_alloc), per_thread_allocs(rend_alloc.get()), rend_stages(rend_alloc.get()),
      base_rend_data(rend_alloc.get()) {
    p_capabilities.conservative_rasterization = false;
    p_capabilities.ray_tracing = false;
    p_capabilities.ray_query = false;
    p_capabilities.timestamp_query = false;
    p_capabilities.desc_set = false;
}

void RendererSoftware::tick(double p_delta) {
    (void)p_delta;
    // TODO
}
Ref<GlobalRendCache> RendererSoftware::get_global_rend_cache() {
    return MEM_NULL; // TODO
}
Ref<IRendGraph> RendererSoftware::acquire_rend_graph(std::string_view p_resource_key, Ref<IRendGraphBindings> p_bindings) {
    static_cast<void>(p_bindings);
    throw std::runtime_error(std::format("RendererSoftware does not support render graphs: {}.", p_resource_key));
}
void RendererSoftware::activate_rend_graph(Ref<IRendGraph> p_graph) {
    if (p_graph != MEM_NULL) {
        throw std::runtime_error("RendererSoftware does not support render graph activation.");
    }
}
std::future<void> RendererSoftware::add_job(std::function<void()> p_job) {
    (void)p_job;
    // TODO
    return create_completed_job_future();
}
} // namespace WhiteBirdEngine
