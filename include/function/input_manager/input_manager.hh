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
#ifndef WBE_FILE_INPUT_MANAGER_HH
#define WBE_FILE_INPUT_MANAGER_HH

#include "key_codes.hh"
#include "utils/defs.hh"
#include "utils/interface/singleton.hh"
namespace WhiteBirdEngine {

using KeyStatus = uint8_t;

// The key is released.
constexpr KeyStatus KEY_UP = 0b00;
// The key is pressed.
constexpr KeyStatus KEY_DOWN = 0b01;

/**
 * @class InputStatus
 * @brief The input status.
 *
 */
struct InputStatus {
    /**
     * @brief The key's status code.
     */
    KeyStatus status_code;

    /**
     * @brief Is key up.
     *
     * @return True if the key is up. False otherwise.
     */
    bool is_up() const {
        return status_code & KEY_UP;
    }

    /**
     * @brief Is key up.
     *
     * @return True if the key is up. False otherwise.
     */
    bool is_down() const {
        return status_code & KEY_DOWN;
    }
};

/**
 * @class InputManager
 * @brief Manages user inputs.
 *
 */
class InputManager : public Singleton<InputManager> {
public:
    WBE_R6_DELETE_COPY_MOVE_OVERRIDE(InputManager)

    /**
     * @brief Get the input status of a key.
     *
     * @param p_key_code The key code to get input status from.
     * @return The input status.
     */
    virtual InputStatus get_input_status(KeyCode p_key_code) const = 0;

    /**
     * @brief Update the input manager.
     */
    virtual void update() = 0;
};

} // namespace WhiteBirdEngine

#endif
