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
        id = "svo_build_static_mesh",
        source_type = "shader",
        src = "assets/shaders/svo/svo_build_static_mesh.slang",
    },
    {
        id = "svo_build_static_mesh_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_build_static_mesh",
        geometry_stage_id = "svo_build_static_mesh",
        fragment_stage_id = "svo_build_static_mesh",
        depth_test = false,
        depth_write = false,
        cull_backfaces = false,
        conservative_rasterization = false,
    },
    {
        id = "svo_reset_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_reset.slang",
    },
    {
        id = "svo_deferred_count_reset_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_deferred_count_reset.slang",
        -- Without the macro the reset clears the pong count; with it, the ping count.
        variants = {
            { name = "default" },
            { name = "reset_ping_count", values = { { name = "WBE_RESET_PING_COUNT", value = "1" } } },
        },
    },
    {
        id = "svo_build_deferred_propagation_pong_write_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_build_deferred_propagation.slang",
    },
    {
        id = "svo_build_deferred_propagation_ping_write_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_build_deferred_propagation.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_USE_PING_WRITE", value = "1" } },
            },
        },
    },
    {
        id = "svo_dynamic_build_static_mesh",
        source_type = "shader",
        src = "assets/shaders/svo/svo_build_static_mesh.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_DYNAMIC_BUILD", value = "1" }, { name = "WBE_MESH_TRANSFORM", value = "1" } },
            },
        },
    },
    {
        id = "svo_dynamic_build_static_mesh_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_dynamic_build_static_mesh",
        geometry_stage_id = "svo_dynamic_build_static_mesh",
        fragment_stage_id = "svo_dynamic_build_static_mesh",
        depth_test = false,
        depth_write = false,
        cull_backfaces = false,
    },
    {
        id = "svo_dynamic_build_deferred_propagation_pong_write_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_build_deferred_propagation.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_DYNAMIC_BUILD", value = "1" } },
            },
        },
    },
    {
        id = "svo_dynamic_build_deferred_propagation_ping_write_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_build_deferred_propagation.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_USE_PING_WRITE", value = "1" }, { name = "WBE_DYNAMIC_BUILD", value = "1" } },
            },
        },
    },
    {
        id = "svo_pre_deferred_debug_capture_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_pre_deferred_debug_capture.slang",
    },
    {
        id = "svo_debug_draw_setup_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_debug_draw_setup.slang",
    },
    {
        id = "svo_debug_draw_find_nodes_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_debug_draw_find_nodes.slang",
    },
    {
        id = "svo_debug_draw_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_debug_draw.slang",
        -- WBE_SVO_DEBUG_DRAW_SOLID draws shaded faces instead of edges; WBE_SVO_DEBUG_DRAW_OCCUPANCY colors by occupancy.
        variants = {
            { name = "default" },
            { name = "solid", values = { { name = "WBE_SVO_DEBUG_DRAW_SOLID", value = "1" } } },
            { name = "occupancy", values = { { name = "WBE_SVO_DEBUG_DRAW_OCCUPANCY", value = "1" } } },
            {
                name = "solid_occupancy",
                values = {
                    { name = "WBE_SVO_DEBUG_DRAW_SOLID", value = "1" },
                    { name = "WBE_SVO_DEBUG_DRAW_OCCUPANCY", value = "1" },
                },
            },
        },
    },
    {
        id = "svo_debug_draw_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_debug_draw_shader",
        fragment_stage_id = "svo_debug_draw_shader",
        depth_test = true,
        depth_write = false,
        cull_backfaces = false,
    },
    -- The post-construction tree drawn as shaded faces.
    {
        id = "svo_debug_draw_solid_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_debug_draw_shader",
        vertex_stage_variant = "solid",
        fragment_stage_id = "svo_debug_draw_shader",
        fragment_stage_variant = "solid",
        depth_test = true,
        depth_write = true,
        cull_backfaces = false,
    },
    -- The post-construction tree drawn as edges, colored by occupancy.
    {
        id = "svo_debug_draw_occupancy_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_debug_draw_shader",
        vertex_stage_variant = "occupancy",
        fragment_stage_id = "svo_debug_draw_shader",
        fragment_stage_variant = "occupancy",
        depth_test = true,
        depth_write = false,
        cull_backfaces = false,
    },
    -- The post-construction tree drawn as shaded faces, colored by occupancy.
    {
        id = "svo_debug_draw_solid_occupancy_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_debug_draw_shader",
        vertex_stage_variant = "solid_occupancy",
        fragment_stage_id = "svo_debug_draw_shader",
        fragment_stage_variant = "solid_occupancy",
        depth_test = true,
        depth_write = true,
        cull_backfaces = false,
    },
    {
        id = "svo_pre_deferred_debug_draw_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_pre_deferred_debug_draw.slang",
    },
    {
        id = "svo_pre_deferred_debug_draw_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "svo_pre_deferred_debug_draw_shader",
        fragment_stage_id = "svo_pre_deferred_debug_draw_shader",
        depth_test = true,
        depth_write = false,
        cull_backfaces = false,
    },
    {
        id = "svo_dynamic_clear_shader",
        source_type = "shader",
        src = "assets/shaders/svo/svo_clear_dynamic.slang",
    },
}
