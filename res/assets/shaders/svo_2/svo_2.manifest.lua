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
            id = "svo_2_build_static_mesh",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_build_static_mesh.slang",
            depth_test = false,
            cull_backfaces = false,
        },
        {
            id = "svo_2_build_static_mesh_pipeline",
            type = "graphics_pipeline",
            vertex_stage_id = "svo_2_build_static_mesh",
            geometry_stage_id = "svo_2_build_static_mesh",
            fragment_stage_id = "svo_2_build_static_mesh",
            depth_test = false,
            depth_write = false,
            cull_backfaces = false,
            conservative_rasterization = false,
        },
        {
            id = "svo_2_reset_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_reset.slang",
        },
        {
            id = "svo_2_build_deferred_propagation_pong_write_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_build_deferred_propagation.slang",
        },
        {
            id = "svo_2_build_deferred_propagation_ping_write_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_build_deferred_propagation.slang",
            variants = {
                {
                    name = "default",
                    values = { { name = "WBE_USE_PING_WRITE", value = "1" } },
                },
            },
        },
        {
            id = "svo_2_update_states_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_update_states.slang",
        },
        {
            id = "svo_2_dynamic_build_static_mesh",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_build_static_mesh.slang",
            depth_test = false,
            cull_backfaces = false,
            variants = {
                {
                    name = "default",
                    values = { { name = "WBE_DYNAMIC_BUILD", value = "1" }, { name = "WBE_MESH_TRANSFORM", value = "1" } },
                },
            },
        },
        {
            id = "svo_2_dynamic_build_static_mesh_pipeline",
            type = "graphics_pipeline",
            vertex_stage_id = "svo_2_dynamic_build_static_mesh",
            geometry_stage_id = "svo_2_dynamic_build_static_mesh",
            fragment_stage_id = "svo_2_dynamic_build_static_mesh",
            depth_test = false,
            depth_write = false,
            cull_backfaces = false,
            conservative_rasterization = false,
        },
        {
            id = "svo_2_dynamic_build_deferred_propagation_pong_write_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_build_deferred_propagation.slang",
            variants = {
                {
                    name = "default",
                    values = { { name = "WBE_DYNAMIC_BUILD", value = "1" } },
                },
            },
        },
        {
            id = "svo_2_dynamic_build_deferred_propagation_ping_write_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_build_deferred_propagation.slang",
            variants = {
                {
                    name = "default",
                    values = { { name = "WBE_USE_PING_WRITE", value = "1" }, { name = "WBE_DYNAMIC_BUILD", value = "1" } },
                },
            },
        },
        {
            id = "svo_2_dynamic_update_states_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_dynamic_update_states.slang",
        },
        {
            id = "svo_2_dynamic_clear_shader",
            source_type = "shader",
            src = "assets/shaders/svo_2/svo_clear_dynamic.slang",
        },
    },
}
