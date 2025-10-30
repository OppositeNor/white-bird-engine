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
#ifndef __WBE_APPLICATION_GAME_HH__
#define __WBE_APPLICATION_GAME_HH__

#include "core/memory/reference_strong.hh"
#include "function/application/application.hh"
#include "function/render/renderer.hh"
namespace WhiteBirdEngine {

/**
 * @class ApplicationGame
 * @brief Application class to run the game.
 *
 */
class ApplicationGame : public Application {
public:
    ApplicationGame();
    virtual ~ApplicationGame() {}
    ApplicationGame(const ApplicationGame&) = delete;
    ApplicationGame(ApplicationGame&&) = delete;
    ApplicationGame& operator=(const ApplicationGame&) = delete;
    ApplicationGame& operator=(ApplicationGame&&) = delete;

    virtual void render_tick(double p_delta) override;
    virtual void logic_tick(double p_delta) override;

    /**
     * @brief Get the allocator for the renderer.
     *
     * @return The allocator for the renderer.
     */
    Ref<RenderAllocator> get_renderer_allocator() const {
        return renderer_allocator;
    }

    Ref<Renderer> get_renderer() const {
        return renderer;
    }

private:
    Ref<RenderAllocator> renderer_allocator;
    void init();
protected:
    Ref<Renderer> renderer;
};

}

#endif
