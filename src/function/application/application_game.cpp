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
#include "function/application/application_game.hh"
#include "core/engine_core.hh"
#include "core/job/job_buffer_ring_spsc.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "function/engine_function.hh"
#include "function/render/render_utils.hh"
#include "function/render/renderer.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_dummy/renderer_dummy.hh"
#include <atomic>
#include <format>
#include <thread>

namespace WhiteBirdEngine {

ApplicationGame::ApplicationGame() {
    init();
}

void ApplicationGame::run() {
    std::jthread render_thread([this]() {
        run_render();
    });
}

void ApplicationGame::init() {
    const auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    // Initialize the deltas into a very small number.
    render_delta.store(0.0001, std::memory_order_seq_cst);
    logic_delta.store(0.0001, std::memory_order_seq_cst);

    render_job_buffer = make_ref<JobBufferRingSPSC<JobRender>>(global_allocator(), global_allocator(), 64);
    renderer_allocator = make_ref<RenderAllocator>(global_allocator(), WBE_MiB(0.6));
    if (game_config.render_api == "vulkan") {
        renderer = make_ref<RendererVK>(global_allocator(), renderer_allocator.get(), render_job_buffer);
    }
    else if (game_config.render_api == "dummy") {
        renderer = make_ref<RendererDummy>(global_allocator(), renderer_allocator.get());
    }
    else {
        wbe_console_log(WBE_CHANNEL_RENDER)->warning(
            std::format("Unrecognized renderer: \"{}\", using a dummy renderer.", game_config.render_api));
        renderer = make_ref<RendererDummy>(global_allocator(), renderer_allocator.get());
    }
}

void ApplicationGame::run_render() {
    bool local_should_close = false;
    auto* global_clock = EngineCore::get_singleton()->global_clock;
    double start_time, end_time;
    while (!local_should_close) {
        start_time = global_clock->get_duration();
        renderer->pool_events(local_should_close, input_manager);
        should_close.store(local_should_close, std::memory_order_release);
        renderer->tick(render_delta.load(std::memory_order_relaxed));
        end_time =  global_clock->get_duration();
        render_delta.store(end_time - start_time, std::memory_order_release);
    }
}

void ApplicationGame::run_logic() {
    bool local_should_close = false;
    while (!local_should_close) {
        // TODO
        local_should_close = should_close.load(std::memory_order_acquire);
    }
}

} // namespace WhiteBirdEngine
