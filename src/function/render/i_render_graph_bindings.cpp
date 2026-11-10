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
#include "function/render/i_render_graph_bindings.hh"
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

IRenderGraphBindings::IRenderGraphBindings(std::string_view p_instance_key) : instance_key(p_instance_key) {
    if (instance_key.empty()) {
        throw std::runtime_error("Render graph bindings require a non-empty instance key.");
    }
}

} // namespace WhiteBirdEngine
