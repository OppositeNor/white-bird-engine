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
#ifndef WBE_FILE_RENDER_GRAPH_RESOURCE_BINDING_HH
#define WBE_FILE_RENDER_GRAPH_RESOURCE_BINDING_HH

#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_task.hh"
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class RenderGraphResourceBinding
 * @brief A backend-neutral named render graph resource override.
 */
struct RenderGraphResourceBinding {
    /** @brief Resource id used by the render graph descriptor; valid for the duration of the update call. */
    std::string_view resource_id;
    /** @brief Strong reference to the backend-neutral resource override. */
    Ref<IRenderObject> resource;
};

} // namespace WhiteBirdEngine

#endif
