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

-- Exercise the typed graph constructors through both the ACP type checker and the script backend.
local type wbe = require("wbe")
local graph_utils = require("utils.render_graph")
local exports = {}

function exports.build(): wbe.RendGraphRecipeVK
    local image = graph_utils.image("color_image", "image_read_write", "r16g16b16a16_sfloat",
        { width = 16, height = 8 }, nil, 4)
    local color = image:view()
    local mip_chain = image:view("color_mips", { mip_level_count = 4 })
    local buffer = graph_utils.storage_buffer("commands", 64)
    local input = graph_utils.import_resource("input")
    local graph = graph_utils.graph()
    local prepare = graph:compute("prepare", { pipeline = "prepare_pipeline", group_count_x = 2 }, {
        aliases = { shader_input = input },
        trigger = { event = "prepare_event", activate = true },
        toggle = { event = "prepare_toggle", enabled = true },
    })
    local draw = graph:draw("draw", { pipeline = "draw_pipeline", draw_vertex_count = 3 })
    draw:color_target(0, color)
    graph:draw_mesh("mesh", { mesh = "mesh_key", pipeline = "mesh_pipeline", parts = { "opaque" }, transform = buffer })
        :color_target(0, color)
    graph:draw_indirect("draw_indirect", { pipeline = "draw_indirect_pipeline", index_buffer = buffer,
        indirect_buffer = buffer }):color_target(0, color)
    graph:compute_indirect("compute_indirect", { pipeline = "compute_indirect_pipeline", indirect_buffer = buffer })
    graph:compute_sequential("sequence", {
        queue_family = "compute_1",
        dispatches = {
            { pipeline = "direct_pipeline", group_count_x = 2 },
            { pipeline = "indirect_pipeline", indirect_buffer = buffer, aliases = { shader_buffer = buffer } },
            { pipeline = "local_pipeline", indirect_buffer = "commands_local", aliases = { shader_buffer = "commands_local" } },
        },
    }, { aliases = { commands_local = buffer } })
    graph:generate_mipmaps("mipmaps", { source_view = color, mip_chain_view = mip_chain, format = "r16g16b16a16_sfloat" })
    graph:cpu_acquire_image("readback", { image_view = color })
    graph:full_memory_barrier("barrier", { resource = buffer })
    graph:ping_pong("ping_pong", { ping_pipeline = "ping_pipeline", pong_pipeline = "pong_pipeline", iterations = 2 })
    local overlay = graph:import_task("overlay")
    graph:chain({ color, buffer }, { prepare, draw, overlay })
    prepare:connect_all({ draw, overlay }, color)
    draw:inputs_from(prepare, { typed_input = buffer })
    return graph:build()
end

return exports
