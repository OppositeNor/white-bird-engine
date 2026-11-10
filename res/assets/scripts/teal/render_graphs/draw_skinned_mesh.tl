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

-- Draws the skinned SimpleSkin mesh into the swap chain, then the application's ImGui overlay. See
-- ApplicationVulkanDrawSkinnedMeshTest in tests/executable/function/render/vulkan_draw_skinned_mesh_test.cpp.
-- Imports: `swap_chain`, which the renderer supplies, the application's `skinned_mesh_camera_info` uniform buffer and
-- `skinned_mesh_skinning_transforms` storage buffer, and its `draw_skinned_mesh_imgui` task.

local type wbe = require("wbe")
local render_graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.RendRecipeContextVK, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local graph = render_graph.new()
    local swap_chain = graph:import_resource("swap_chain")
    local camera_info = graph:import_resource("skinned_mesh_camera_info")
    local skinning_transforms = graph:import_resource("skinned_mesh_skinning_transforms")
    -- The shader reads position from the geometry binding and the joint ids and weights from the joint-binding binding,
    -- which the draw binds from the compiled mesh.
    local mesh = graph:register_mesh("skinned_mesh", { mesh = "model_simple_skin.mesh" })
    local pipeline = graph:register_graphics_pipeline("draw_skinned_mesh_pipeline", {
        pipeline = "draw_skinned_mesh_pipeline",
        color_formats = { context.swap_chain_format },
    })
    local draw = graph:draw_mesh("draw_skinned_mesh", {
        mesh = mesh,
        pipeline = { default = pipeline },
        parts = { "opaque" },
        clear_values = { { color = { 0.05, 0.05, 0.05, 1.0 } } },
    }, { uses = { camera_info, skinning_transforms } }):color_target(0, swap_chain)
    local imgui = graph:import_task("draw_skinned_mesh_imgui")
    graph:chain(swap_chain, { draw, imgui })
    return graph:build()
end

return exports
