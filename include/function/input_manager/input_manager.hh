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

#include "function/input_manager/key_codes.hh"
#include "utils/defs.hh"
#include "utils/interface/i_singleton.hh"
#include <atomic>
#include <cstdint>
#include <type_traits>

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
    bool is_up() const { return status_code & KEY_UP; }

    /**
     * @brief Is key up.
     *
     * @return True if the key is up. False otherwise.
     */
    bool is_down() const { return status_code & KEY_DOWN; }
};
/**
 * @class InputManager
 * @brief Input manager for SDL backend.
 *
 */
class InputManager final : public ISingleton<InputManager> {
public:
    InputManager();
    ~InputManager();
    WBE_R6_NDCD_DELETE_COPY_MOVE(InputManager)

    /**
     * @brief Get the singleton instance.
     *
     * @return The singleton instance.
     */
    static InputManager* get_singleton() { return singleton; }

    /**
     * @brief Get the input status of a key.
     *
     * @param p_key_code The key code.
     * @return The input status.
     */
    InputStatus get_input_status(KeyCode p_key_code) const;

    /**
     * @brief Set the keycode to key down.
     */
    void set_keydown(KeyCode p_key_code);

    /**
     * @brief Set the keycode to key up.
     */
    void set_keyup(KeyCode p_key_code);

private:
    // Use an atomic keyset to store the key statuses.
    std::atomic<uint8_t> key_status_map[KeyCode::TOTAL_KEYCODES % 8 == 0
                                            ? KeyCode::TOTAL_KEYCODES / 8
                                            : KeyCode::TOTAL_KEYCODES / 8 + 1] =
        {};
    static_assert(
        std::remove_cvref_t<decltype(key_status_map[0])>::is_always_lock_free,
        "Key status map must be lock free");
    inline static InputManager* singleton = nullptr;
};
} // namespace WhiteBirdEngine

#endif
