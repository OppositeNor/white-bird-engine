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
#ifndef WBE_FILE_RENDER_TEST_HH
#define WBE_FILE_RENDER_TEST_HH
#include "core/reflection/reflection_defs.hh"

namespace WhiteBirdEngine {
WBE_LABEL(RT_RUN_PIPELINE, WBE_RENDER_TASK);
WBE_LABEL(RT_RUN_PIPELINE_SHADOWMAP, WBE_RENDER_TASK);
WBE_LABEL(REND_RES_TEST_DRAW_CUBE_GRAPHICS_PIPELINE, WBE_RENDER_RESOURCE);
WBE_LABEL(REND_RES_TEST_SHADOWMAP_GRAPHICS_PIPELINE, WBE_RENDER_RESOURCE);
WBE_LABEL(REND_RES_TEST_SHADOWMAP_FRAMEBUFFER, WBE_RENDER_RESOURCE);

} // namespace WhiteBirdEngine

#endif
