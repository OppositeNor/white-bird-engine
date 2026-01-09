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
#ifndef WBE_FILE_INPUT_MANAGER_SDL_HH
#define WBE_FILE_INPUT_MANAGER_SDL_HH

#include "SDL_events.h"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "utils/defs.hh"
#include <bitset>
namespace WhiteBirdEngine {
/**
 * @class InputManagerSDL
 * @brief Input manager for SDL backend.
 *
 */
class InputManagerSDL final : public InputManager {
public:
    InputManagerSDL();
    virtual ~InputManagerSDL() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(InputManagerSDL)

    /**
     * @brief Get the singleton instance.
     *
     * @return The singleton instance.
     */
    static InputManagerSDL* get_singleton() {
        return singleton;
    }

    /**
     * @brief Get the input status of a key.
     *
     * @param p_key_code The key code.
     * @return The input status.
     */
    virtual InputStatus get_input_status(KeyCode p_key_code) const override;

    /**
     * @brief Update the input manager.
     */
    virtual void update() override;

private:
    std::bitset<KeyCode::TOTAL_KEYCODES * 2> key_status_map; 
    inline static InputManagerSDL* singleton = nullptr;
    SDL_Event sdl_event = {};
};
} // namespace WhiteBirdEngine

#endif
