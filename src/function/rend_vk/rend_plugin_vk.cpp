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
#include "function/rend_vk/rend_plugin_vk.hh"
#include "core/alloc/stl_alloc.hh"
#include "function/render/i_renderer.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/desc_set_plugin_vk.hh"
#include "function/rend_vk/ray_tracing_plugin_vk.hh"
#include "function/rend_vk/timestamp_query_plugin_vk.hh"

#include "core/memory/reference_strong.hh"
#include "function/rend_vk/rend_context_vk.hh"

namespace WhiteBirdEngine {

void register_builtin_rend_plugins_vk(Vector<Ref<RendPluginVK>, RendAlloc>& p_registered_plugins,
    RendAlloc* p_alloc,
    const RendContextVK* p_rend_data,
    const RendererCapabilities& p_capabilities) {
    p_registered_plugins.clear();
    if (p_capabilities.ray_tracing || p_capabilities.ray_query) {
        p_registered_plugins.emplace_back(make_ref<RayTracingPluginVK>(p_alloc, p_rend_data));
    }
    if (p_capabilities.timestamp_query) {
        p_registered_plugins.emplace_back(make_ref<TimestampQueryPluginVK>(p_alloc, p_rend_data));
    }
    if (p_capabilities.desc_set) {
        p_registered_plugins.emplace_back(make_ref<DescSetPluginVK>(p_alloc, p_rend_data));
    }
}

} // namespace WhiteBirdEngine
