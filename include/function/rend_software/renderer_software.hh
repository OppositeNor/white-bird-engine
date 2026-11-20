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
#ifndef WBE_FILE_RENDERER_SOFTWARE
#define WBE_FILE_RENDERER_SOFTWARE

#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_rend_cache.hh"
#include "function/render/i_rend_graph.hh"
#include "function/render/i_rend_graph_bindings.hh"
#include "function/render/i_renderer.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_software/rend_software_defs.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <string_view>
#include <thread>
namespace WhiteBirdEngine {

/**
 * @class RendererSoftware
 * @brief Software renderer without render graph support.
 */
class RendererSoftware final : public IRenderer {
public:
    static constexpr uint32_t PER_THREAD_ALLOC_SIZE = WBE_MI_B(1);

    virtual ~RendererSoftware() override = default;
    RendererSoftware(const RendererSoftware&) = delete;
    RendererSoftware(RendererSoftware&&) = delete;
    RendererSoftware& operator=(const RendererSoftware&) = delete;
    RendererSoftware& operator=(RendererSoftware&&) = delete;

    RendererSoftware(Ref<RendAlloc> p_alloc, RendererCapabilities& p_capabilities);

    virtual uint32_t get_worker_nums() override {
        // Leave one thread for logic and one thread for worker thread.
        return std::max(std::thread::hardware_concurrency() - 2, 1U);
    }
    virtual void tick(double p_delta) override;
    virtual Ref<GlobalRendCache> get_global_rend_cache() override;
    virtual Ref<IRendGraph> acquire_rend_graph(std::string_view p_resource_key, Ref<IRendGraphBindings> p_bindings) override;
    virtual void activate_rend_graph(Ref<IRendGraph> p_graph) override;
    virtual std::future<void> add_job(std::function<void()> p_job) override;

private:
    Ref<RendAlloc> rend_alloc;
    Vector<Ref<RendAlloc>, RendAlloc> per_thread_allocs;
    Vector<void (*)(), RendAlloc> rend_stages;

    BaseRendDataSoftware base_rend_data;

    void init();
    void setup_base_rend_data();
};
} // namespace WhiteBirdEngine

#endif
