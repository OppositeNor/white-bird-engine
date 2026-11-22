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
    {
        id = "shadowmap_spot_shader",
        source_type = "shader",
        src = "assets/shaders/shadowmap_spot/shadowmap_spot.slang",
    },
    {
        id = "shadowmap_spot",
        type = "graphics_pipeline",
        vertex_stage_id = "shadowmap_spot_shader",
        fragment_stage_id = "shadowmap_spot_shader",
        depth_test = true,
        depth_write = true,
        color_blend = false,
    },
    {
        id = "shadowmap_spot_masked_shader",
        source_type = "shader",
        src = "assets/shaders/shadowmap_spot/shadowmap_spot_masked.slang",
    },
    {
        id = "shadowmap_spot_masked",
        type = "graphics_pipeline",
        vertex_stage_id = "shadowmap_spot_masked_shader",
        fragment_stage_id = "shadowmap_spot_masked_shader",
        depth_test = true,
        depth_write = true,
        cull_backfaces = false,
        color_blend = false,
    },
    {
        id = "rsm_spot_shader",
        source_type = "shader",
        src = "assets/shaders/shadowmap_spot/rsm_spot.slang",
        -- WBE_MESH_TRANSFORM applies the entity's mesh transform, for placed meshes.
        variants = {
            { name = "default" },
            { name = "mesh_transform", values = { { name = "WBE_MESH_TRANSFORM", value = "1" } } },
        },
    },
    {
        id = "rsm_spot_dynamic",
        type = "graphics_pipeline",
        vertex_stage_id = "rsm_spot_shader",
        vertex_stage_variant = "mesh_transform",
        fragment_stage_id = "rsm_spot_shader",
        fragment_stage_variant = "mesh_transform",
        fragment_stage_entry = "fragment_main",
        depth_test = true,
        depth_write = true,
        color_blend = false,
    },
    {
        id = "rsm_spot",
        type = "graphics_pipeline",
        vertex_stage_id = "rsm_spot_shader",
        fragment_stage_id = "rsm_spot_shader",
        fragment_stage_entry = "fragment_main",
        depth_test = true,
        depth_write = true,
        color_blend = false,
    },
    {
        id = "rsm_spot_masked_shader",
        source_type = "shader",
        src = "assets/shaders/shadowmap_spot/rsm_spot_masked.slang",
    },
    {
        id = "rsm_spot_masked",
        type = "graphics_pipeline",
        vertex_stage_id = "rsm_spot_masked_shader",
        fragment_stage_id = "rsm_spot_masked_shader",
        depth_test = true,
        depth_write = true,
        cull_backfaces = false,
        color_blend = false,
    },
    {
        id = "rsm_spot_opaque_double_sided",
        type = "graphics_pipeline",
        vertex_stage_id = "rsm_spot_shader",
        fragment_stage_id = "rsm_spot_shader",
        depth_test = true,
        depth_write = true,
        color_blend = false,
        cull_backfaces = false,
        fragment_stage_entry = "fragment_double_sided_main",
    },
    {
        id = "shadowmap_spot_opaque_double_sided",
        type = "graphics_pipeline",
        vertex_stage_id = "shadowmap_spot_shader",
        fragment_stage_id = "shadowmap_spot_shader",
        depth_test = true,
        depth_write = true,
        color_blend = false,
        cull_backfaces = false,
    },
}
