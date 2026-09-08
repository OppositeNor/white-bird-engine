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
#include "core/async/worker.hh"
#include "core/engine_core.hh"
#include "core/job/job_buffer_ring_mpsc.hh"
#include "core/logging/log.hh"
#include "core/memory/reference_strong.hh"
#include "function/game_config/game_config.hh"
#include "function/input_manager/input_manager.hh"
#include "function/render/i_renderer.hh"
#include "function/render/render_defs.hh"
#include "function/render_dummy/renderer_dummy.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/window/i_window.hh"
#include "function/window/window_sdl.hh"
#include "resource/resource_manager/resource_manager.hh"
#include <atomic>
#include <chrono>
#include <format>
#include <thread>

namespace WhiteBirdEngine {

namespace {

constexpr double LOGIC_FRAME_INTERVAL_SEC = 1.0 / 60.0;
constexpr size_t LOGIC_THREAD_COUNT = 1;

size_t get_worker_thread_count(size_t p_configured_thread_count, size_t p_render_thread_count) {
    if (p_configured_thread_count > 0) {
        return p_configured_thread_count;
    }

    const size_t cpu_thread_count = std::thread::hardware_concurrency();
    if (cpu_thread_count <= p_render_thread_count + LOGIC_THREAD_COUNT) {
        return 1;
    }
    return cpu_thread_count - p_render_thread_count - LOGIC_THREAD_COUNT;
}

} // namespace

ApplicationGame::ApplicationGame() {
    init();
}

void ApplicationGame::run() {
    std::jthread render_thread([this]() {
        run_render();
    });
    std::jthread logic_thread([this]() {
        run_logic();
    });
}

void ApplicationGame::init() {
    const auto& game_config = GameConfig::get_singleton()->get_config_options();
    const auto& engine_config = EngineCore::get_singleton()->engine_config->get_config_options();
    // Initialize the deltas into a very small number.
    render_delta.store(0.0001, std::memory_order_seq_cst);
    logic_delta.store(0.0001, std::memory_order_seq_cst);

    render_job_buffer = make_ref<JobBufferRingMPSC>(global_allocator(), global_allocator(), 64);
    renderer_allocator = make_ref<RenderAllocator>(global_allocator(), game_config.render_mem_pool_size);
    input_manager = make_ref<InputManager>(global_allocator());
    window = make_ref<WindowSDL>(global_allocator(), input_manager, get_window_properties_from_game_config_options(game_config));
    if (game_config.render_api == "vulkan") {
        renderer = make_ref<RendererVK>(global_allocator(), renderer_allocator.get(), window, render_job_buffer, renderer_capabilities);
    } else if (game_config.render_api == "dummy") {
        renderer = make_ref<RendererDummy>(global_allocator(), renderer_allocator.get(), renderer_capabilities);
    } else {
        stdout_log(WBE_CHANNEL_RENDER)
            ->warning(std::format("Unrecognized renderer: \"{}\", using a dummy renderer.", game_config.render_api));
        renderer = make_ref<RendererDummy>(global_allocator(), renderer_allocator.get(), renderer_capabilities);
    }
    // TEST
    if (false) {
        worker = make_ref<Worker>(
                global_allocator(), get_worker_thread_count(engine_config.worker_thread_count, renderer->get_worker_nums()));
    }
    worker = make_ref<Worker>(
        global_allocator(), 1);
    // audio_renderer = make_ref<AudioRendererMA>(global_allocator());
    ResourceManager::get_singleton()->load_global_resource("res_chunks/global_resources.json");
}

void ApplicationGame::run_render() {
    bool local_should_close = false;
    auto* global_clock = EngineCore::get_singleton()->global_clock;
    double start_time, end_time;
    while (!local_should_close) {
        start_time = global_clock->get_duration();
        render_update(render_delta.load(std::memory_order_relaxed));
        window->tick_render();
        renderer->tick(render_delta.load(std::memory_order_relaxed));
        end_time = global_clock->get_duration();
        render_delta.store(end_time - start_time, std::memory_order_release);
        local_should_close = should_close.load(std::memory_order_acquire);
    }
}

void ApplicationGame::run_logic() {
    bool local_should_close = false;
    using LogicClock = std::chrono::steady_clock;
    const auto logic_frame_interval = std::chrono::duration<double>(LOGIC_FRAME_INTERVAL_SEC);
    while (!local_should_close) {
        LogicClock::time_point frame_start = LogicClock::now();
        local_should_close = window->should_close();
        should_close.store(local_should_close, std::memory_order_release);
        window->tick_logic();
        logic_update(logic_delta.load(std::memory_order_relaxed));
        LogicClock::time_point frame_end = LogicClock::now();
        auto frame_duration = frame_end - frame_start;
        if (frame_duration < logic_frame_interval) {
            std::this_thread::sleep_until(frame_start + logic_frame_interval);
            frame_end = LogicClock::now();
            frame_duration = frame_end - frame_start;
        }
        logic_delta.store(std::chrono::duration<double>(frame_duration).count(), std::memory_order_release);
    }
    worker->wait();
}

WindowProperties ApplicationGame::get_window_properties_from_game_config_options(const GameConfigOptions& p_game_config_options) {
    WindowProperties properties{
        .title = p_game_config_options.game_name,
        .width = p_game_config_options.window_width,
        .height = p_game_config_options.window_height,
        .is_resizable = false,  // TODO
        .is_full_screen = false // TODO
    };
    return properties;
}

} // namespace WhiteBirdEngine
