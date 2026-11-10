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

-- Draws the triangle mesh straight into the swap chain with the animated triangle pipeline.
-- Imports: `swap_chain`, which the renderer supplies.

local type wbe = require("wbe")
local render_graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(context: wbe.RendRecipeContextVK, _parameters: {string:any}): wbe.RendGraphRecipeVK
    local graph = render_graph.new()
    local swap_chain = graph:import_resource("swap_chain")
    local triangle = graph:register_mesh("triangle", { mesh = "triangle" })
    local pipeline = graph:register_graphics_pipeline("draw_triangle_pipeline", {
        pipeline = "draw_triangle_pipeline",
        color_formats = { context.swap_chain_format },
    })
    graph:draw_mesh("run_pipeline", {
        mesh = triangle,
        pipeline = { default = pipeline },
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 1.0 } } },
    }):color_target(0, swap_chain)
    return graph:build()
end

return exports
