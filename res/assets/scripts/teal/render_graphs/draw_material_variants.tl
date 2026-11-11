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

-- Draws a mesh whose submeshes use materials of two pipeline variants with one draw task: the default variant with
-- `draw_cube_unshaded_pipeline` and the "tinted" variant with `draw_cube_unshaded_tinted_pipeline`.
-- Imports: `swap_chain` (the swap chain image views, which the renderer supplies) and `camera_info` (the application's
-- camera uniform buffer).

local type wbe = require("wbe")
local graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.RendRecipeContextVK, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local function pipeline(key: string): graph.Resource
        return graph.graphics_pipeline(key, { pipeline = key, color_formats = { context.swap_chain_format } })
    end
    local draw = graph.draw_mesh({
        mesh = graph.mesh("material_variants.cube"),
        pipeline = {
            default = pipeline("draw_cube_unshaded_pipeline"),
            tinted = pipeline("draw_cube_unshaded_tinted_pipeline"),
        },
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } } },
    }, {
        name = "draw_material_variants",
        bindings = { camera_info = graph.import("camera_info") },
    }):color_target(0, graph.import("swap_chain"))
    return graph.build({ tasks = { draw } })
end

return exports
