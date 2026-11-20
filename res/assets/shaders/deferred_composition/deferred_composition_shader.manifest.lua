-- Copyright 2025 OppositeNor
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
return {
    resources = {
        {
            id = "shadow_moments_summed_area_table_horizontal_shader",
            source_type = "shader",
            src = "assets/shaders/deferred_composition/shadow_moments_summed_area_table_horizontal.slang",
        },
        {
            id = "shadow_moments_summed_area_table_vertical_shader",
            source_type = "shader",
            src = "assets/shaders/deferred_composition/shadow_moments_summed_area_table_vertical.slang",
        },
        {
            id = "direct_lighting_shader",
            source_type = "shader",
            src = "assets/shaders/deferred_composition/direct_lighting.slang",
            -- WBE_USE_SUMMED_AREA_TABLE_SHADOW_MOMENTS filters the shadow moments through their summed-area table.
            macros = { { macro = "WBE_USE_SUMMED_AREA_TABLE_SHADOW_MOMENTS", options = { "1" }, optional = true } },
        },
        {
            id = "indirect_lighting_rsm_shader",
            source_type = "shader",
            src = "assets/shaders/deferred_composition/indirect_lighting_rsm.slang",
            -- WBE_VOXEL_DEBUG_VIEW outputs the voxel debug view instead of the indirect lighting.
            macros = { { macro = "WBE_VOXEL_DEBUG_VIEW", options = { "1" }, optional = true } },
        },
        {
            id = "combine_lighting_shader",
            source_type = "shader",
            src = "assets/shaders/deferred_composition/combine_lighting.slang",
        },
        {
            id = "present_post_processing_shader",
            source_type = "shader",
            src = "assets/shaders/deferred_composition/present_post_processing.slang",
        },
        {
            id = "present_post_processing",
            type = "graphics_pipeline",
            vertex_stage_id = "present_post_processing_shader",
            fragment_stage_id = "present_post_processing_shader",
            depth_test = false,
            depth_write = false,
        },
    },
}
