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

-- Exercise the typed task constructors through both the ACP type checker and the script backend.
local type wbe = require("wbe")
local graph = require("render_graph_utils.render_graph_utils")

local exports = {}

function exports.build(): wbe.RendGraphRecipeVK
    local image = graph.image({
        name = "color_image",
        usage = "image_read_write",
        format = "r16g16b16a16_sfloat",
        extent = { width = 16, height = 8 },
        mip_levels = 4,
    })
    local color = image:view()
    local mip_chain = image:view({ name = "color_mips", mip_level_count = 4 })
    local buffer = graph.storage_buffer({ name = "commands", size = 64 })
    local input = graph.import("input")
    local mesh: graph.Resource = graph.mesh("mesh_key")
    local prepare_pipeline: graph.Resource = graph.compute_pipeline("prepare_pipeline")
    local draw_pipeline: graph.Resource = graph.graphics_pipeline("draw_pipeline")
    local direct_pipeline = graph.compute_pipeline("direct_pipeline")
    local indirect_pipeline = graph.compute_pipeline("indirect_pipeline")
    local prepare = graph.compute({ pipeline = prepare_pipeline, group_count_x = 2 }, {
        name = "prepare",
        bindings = { shader_input = input },
        trigger = { event = "prepare_event", activate = true },
        toggle = { event = "prepare_toggle", enabled = true },
    })
    local draw = graph.draw({ pipeline = draw_pipeline, draw_vertex_count = 3 }, { name = "draw" })
    draw:color_target(0, color)
    local mesh_draw = graph.draw_mesh({
        mesh = mesh,
        pipeline = { default = graph.graphics_pipeline("mesh_pipeline") },
        parts = { "opaque" },
        transform = buffer,
    }):color_target(0, color)
    local draw_indirect = graph.draw_indirect({
        pipeline = graph.graphics_pipeline("draw_indirect_pipeline"),
        index_buffer = buffer,
        indirect_buffer = buffer,
    }):color_target(0, color)
    local compute_indirect = graph.compute_indirect({
        pipeline = graph.compute_pipeline("compute_indirect_pipeline"),
        indirect_buffer = buffer,
    })
    local sequential_params: graph.RTParamsRunComputePipelinesSequentialVK = {
        queue_family = "compute_1",
        dispatches = {
            { pipeline = direct_pipeline, group_count_x = 2 },
            { pipeline = indirect_pipeline, indirect_buffer = buffer, bindings = { shader_buffer = buffer } },
            graph.indirect_dispatch(graph.compute_pipeline("local_pipeline"), buffer, 0, { shader_buffer = buffer }),
        },
    }
    local sequence = graph.compute_sequential(sequential_params)
    local mipmaps = graph.generate_mipmaps({ source_view = color, mip_chain_view = mip_chain, format = "r16g16b16a16_sfloat" })
    local readback = graph.cpu_acquire_image({ image_view = color, event = "readback_event" })
    local barrier = graph.full_memory_barrier({ resource = buffer })
    local ping_pong = graph.ping_pong({
        ping_pipeline = graph.compute_pipeline("ping_pipeline"),
        pong_pipeline = graph.compute_pipeline("pong_pipeline"),
        iterations = 2,
    })
    -- The branches become one multiplexer task, which takes their params and shares their bindings.
    local multiplexer = graph.multiplex({
        name = "select",
        branches = {
            graph.compute({ pipeline = direct_pipeline, group_count_x = 4 }, { bindings = { shader_input = input } }),
            graph.compute({ pipeline = indirect_pipeline }, { bindings = { shader_input = input } }),
        },
        select = { event = "select_event", branch = 1 },
        toggle = { event = "select_toggle", enabled = true },
    })
    local overlay = graph.import_task("overlay")
    local outputs = { color, buffer }
    graph.chain(outputs, { prepare, draw, overlay })
    prepare:connect_all({ draw, overlay }, outputs)
    draw:connect(overlay, outputs)
    draw:inputs_from(prepare, { typed_input = buffer })
    return graph.build({
        tasks = { prepare, draw, mesh_draw, draw_indirect, compute_indirect, sequence, mipmaps, readback, barrier, ping_pong,
            multiplexer, overlay },
        exports = { color = color },
    })
end

return exports
