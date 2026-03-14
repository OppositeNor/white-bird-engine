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

#include "function/input_manager/input_manager.hh"

namespace WhiteBirdEngine {

InputManager::InputManager() {
    singleton = this;
}

InputManager::~InputManager() {
    singleton = nullptr;
}

InputStatus InputManager::get_input_status(KeyCode p_key_code) const {
    return InputStatus{
        .status_code = static_cast<uint8_t>(key_status_map[p_key_code / 8].load(std::memory_order_acquire) >> (p_key_code % 8) & 0b1)
    };
}

void InputManager::update_key_status(KeyCode p_key_code, KeyStatus p_key_status) {
    // TOOD
}

} // namespace WhiteBirdEngine
