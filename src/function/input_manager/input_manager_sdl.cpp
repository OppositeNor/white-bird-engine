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

#include "function/input_manager/input_manager_sdl.hh"

namespace WhiteBirdEngine {
InputManagerSDL::InputManagerSDL() {
    singleton = this;
}

InputManagerSDL::~InputManagerSDL() {
    singleton = nullptr;
}

InputStatus InputManagerSDL::get_input_status(KeyCode p_key_code) const {
    return InputStatus{
        .status_code = static_cast<KeyStatus>(
            (key_status_map[p_key_code * 2 + 1] << 1) |
            (key_status_map[p_key_code * 2])
        )
    };
}

void InputManagerSDL::update() {
    // TODO
}

} // namespace WhiteBirdEngine
