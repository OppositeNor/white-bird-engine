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
#ifndef __WBE_INPUT_MANAGER_HH__
#define __WBE_INPUT_MANAGER_HH__

#include "key_codes.hh"
namespace WhiteBirdEngine {

/**
 * @class InputStatus
 * @brief The input status.
 *
 */
struct InputStatus {
    /**
     * @brief Code describing input status.
     */
    enum InputStatusCode {
        // Unknown key status.
        KEY_UNKNOWN = 0b0000,
        // The key is up.
        KEY_UP      = 0b0001,
        // The key is down.
        KEY_DOWN    = 0b0010,
        // The action just happened.
        KEY_JUST    = 0b0100,
        // The key is just up.
        KEY_JUST_UP     = 0b0101,
        // The key is just down.
        KEY_JUST_DOWN   = 0b0110,
    } status_code;

    /**
     * @brief Is the key action just happened.
     *
     * @return True if the key action just happened. False otherwise.
     */
    bool is_just() const {
        return status_code & KEY_JUST;
    }

    /**
     * @brief Is key up.
     *
     * @return True if the key is up. False otherwise.
     */
    bool is_up() const {
        return status_code & KEY_UP;
    }

    /**
     * @brief Is the key just up.
     *
     * @return True if the key just up. False otherwise.
     */
    bool is_just_up() const {
        return status_code == KEY_JUST_UP;
    }

    /**
     * @brief Is key up.
     *
     * @return True if the key is up. False otherwise.
     */
    bool is_down() const {
        return status_code & KEY_DOWN;
    }

    /**
     * @brief Is the key just up.
     *
     * @return True if the key just up. False otherwise.
     */
    bool is_just_down() const {
        return status_code == KEY_JUST_DOWN;
    }
};

/**
 * @class InputManager
 * @brief Manages user inputs.
 *
 */
class InputManager {
public:
    WBE_R6_DEFAULT_VIRTUAL(InputManager)

    /**
     * @brief Get the input status of a key.
     *
     * @param p_key_code The key code to get input status from.
     * @return The input status.
     */
    virtual InputStatus get_input_status(KeyCode p_key_code) const = 0;
};

} // namespace WhiteBirdEngine

#endif
