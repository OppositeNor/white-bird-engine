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
#ifndef WBE_FILE_APPLICATION_GAME_HH
#define WBE_FILE_APPLICATION_GAME_HH

#include "core/memory/reference_strong.hh"
#include "function/audio/audio_renderer.hh"
#include "core/job/job_buffer_ring_spsc.hh"
#include "function/application/application.hh"
#include "function/input_manager/input_manager.hh"
#include "function/render/renderer.hh"
#include "utils/defs.hh"
#include <atomic>
namespace WhiteBirdEngine {

/**
 * @class ApplicationGame
 * @brief Application class to run the game.
 *
 */
class ApplicationGame : public Application {
public:
    ApplicationGame();
    virtual ~ApplicationGame() = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(ApplicationGame)

    virtual void run() override;

    virtual double get_render_delta() const override {
        return render_delta.load(std::memory_order_acquire);
    }

    virtual double get_logic_delta() const override {
        return logic_delta.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the allocator for the renderer.
     *
     * @return The allocator for the renderer.
     */
    Ref<RenderAllocator> get_renderer_allocator() const {
        return renderer_allocator;
    }

    /**
     * @brief Get the renderer instance.
     *
     * @return The renderer instance.
     */
    Ref<Renderer> get_renderer() const {
        return renderer;
    }

    /**
     * @brief Add a job for the renderer.
     */
    void add_render_job(Ref<Job> p_job) {
        render_job_buffer->add_job(p_job);
    }

private:
    Ref<RenderAllocator> renderer_allocator;

    Ref<InputManager> input_manager;

    void init();
    void run_render();
    void run_logic();

    WBE_NO_FALSE_SHARING std::atomic<double> render_delta;
    WBE_NO_FALSE_SHARING std::atomic<double> logic_delta;
    WBE_NO_FALSE_SHARING std::atomic<bool> should_close;
protected:
    Ref<JobBufferRingSPSC> render_job_buffer;
    Ref<Renderer> renderer;
    Ref<AudioRenderer> audio_renderer;

};

} // namespace WhiteBirdEngine

#endif
