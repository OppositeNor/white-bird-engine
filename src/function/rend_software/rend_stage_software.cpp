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
#include "function/rend_software/rend_stage_software.hh"

namespace WhiteBirdEngine {

RendStagesSoftware::RendStagesSoftware()
    : vertex_shading_stage(default_vertex_shading_stage), rasterization_stage(default_rasterization_stage),
      fragment_shading_stage(default_fragment_shading_stage) {
}

RendStagesSoftware::RendStagesSoftware(
    RendStageType p_vertex_shading_stage, RendStageType p_rasterization_stage, RendStageType p_fragment_shading_stage)
    : vertex_shading_stage(p_vertex_shading_stage), rasterization_stage(p_rasterization_stage),
      fragment_shading_stage(p_fragment_shading_stage) {
}

void RendStagesSoftware::default_vertex_shading_stage(RendContextSoftware* p_rend_context) noexcept {
    // TODO
}

void RendStagesSoftware::default_rasterization_stage(RendContextSoftware* p_rend_context) noexcept {
    // TODO
}

void RendStagesSoftware::default_fragment_shading_stage(RendContextSoftware* p_rend_context) noexcept {
    // TODO
}

} // namespace WhiteBirdEngine
