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
#ifndef WBE_FILE_I_WINDOW_HH
#define WBE_FILE_I_WINDOW_HH

#include "core/memory/reference_strong.hh"
#include "function/input_manager/i_input_manager.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class WindowProperties
 * @brief Properties of a window.
 *
 */
struct WindowProperties {
    /**
     * @brief The width of the window.
     */
    uint32_t width;
    /**
     * @brief The height of the window.
     */
    uint32_t height;
    /**
     * @brief Is window full screen.
     */
    bool is_full_screen;
};

/**
 * @class IWindow
 * @brief Window interface.
 *
 */
class IWindow {
public:
    IWindow() = default;
    virtual ~IWindow() = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(IWindow)

    /**
     * @brief Called every render tick.
     */
    virtual void update_render() const = 0;

    /**
     * @brief Called every logic tick.
     */
    virtual void update_logic() const = 0;

    /**
     * @brief Get the input manager.
     */
    virtual Ref<IInputManager> get_input_manager() const = 0;

    /**
     * @brief Get window properties.
     *
     * @return The window properties.
     */
    virtual const WindowProperties& get_window_properties() const = 0;

    /**
     * @brief Get the Vulkan surface instance of this window.
     *
     * @return The Vulkan surface instance.
     */
    virtual VkSurfaceKHR get_vulkan_surface() const = 0;

    /**
     * @brief Should close the window.
     *
     * @return True if the window should be closed. False otherwise.
     */
    virtual bool should_close() const = 0;
};

} // namespace WhiteBirdEngine

#endif
