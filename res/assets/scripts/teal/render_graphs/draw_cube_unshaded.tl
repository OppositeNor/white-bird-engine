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

-- Draws the opaque part of the static cube mesh straight into the swap chain.
-- Imports: `swap_chain` (the swap chain image views) and `camera_info` (the application's camera uniform buffer).

local type wbe = require("wbe")
local graph_utils = require("utils.render_graph")

local exports = {}

function exports.build(_context: wbe.RendRecipeContextVK): wbe.RendGraphRecipeVK
    local swap_chain = graph_utils.import_resource("swap_chain")
    local camera_info = graph_utils.import_resource("camera_info")
    local graph = graph_utils.graph()
    local params: wbe.RTParamsGraphicsDrawMeshVK = {
        mesh = "model_cube_static.mesh",
        pipeline = "draw_cube_unshaded_pipeline",
        parts = { "opaque" },
        clear_values = { { color = { 0.0, 0.0, 0.0, 0.0 } } },
    }
    graph:add({
        id = "run_static_mesh",
        type = "graphics_draw_mesh",
        params = params,
        aliases = { ["render_target_colors[0]"] = swap_chain },
        uses = { camera_info },
    })
    return graph:build()
end

return exports
