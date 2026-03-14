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
#ifndef WBE_FILE_WINDOW_SDL
#define WBE_FILE_WINDOW_SDL

#include "function/input_manager/i_input_manager.hh"
#include "core/memory/reference_strong.hh"
#include "i_window.hh"

namespace WhiteBirdEngine {

/**
 * @class WindowSDL
 * @brief SDL Window.
 *
 */
class WindowSDL final : public IWindow {
public:
    virtual ~WindowSDL() = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(WindowSDL)

    WindowSDL(Ref<IInputManager> p_input_manager);

    virtual void update() const override;
    virtual Ref<IInputManager> get_input_manager() const override;
    virtual const WindowProperties& get_window_properties() const override;
    virtual VkSurfaceKHR get_vulkan_surface() const override;
    virtual bool should_close() const override;

private:
    Ref<IInputManager> input_manager;
};

} // namespace WhiteBirdEngine

#endif
