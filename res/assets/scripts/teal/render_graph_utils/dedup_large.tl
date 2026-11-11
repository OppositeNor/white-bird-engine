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

-- GPU deduplication of large key buffers for render graph recipes, as the dispatches of a
-- run_compute_pipelines_sequential task, which may dispatch other passes before and after them. The radix sort orders the
-- 32-bit keys in place, the first key of every value is marked, and the large prefix scan of the marks gives each marked
-- key its index among the unique keys, to which it is scattered. The application compiles the pipelines under their shader ids
-- (dedup_large_prepare_dispatch, dedup_large_mark, dedup_large_scatter), and those of the radix sort (see
-- render_graph_utils.sort) and the large prefix scan (see render_graph_utils.large_prefix_scan). The key buffer and the
-- unique keys are counted buffers of the keys; see dedup_large_utils.slang and counted_buffer.slang.

local graph = require("render_graph_utils.render_graph_utils")
local sort = require("render_graph_utils.sort")
local large_prefix_scan = require("render_graph_utils.large_prefix_scan")

-- One VkDispatchIndirectCommand (3 uint32), the dispatch of the per-key passes.
local DEDUP_LARGE_DISPATCH_BUFFER_SIZE = 12

local record dedup_large
    -- The buffers of a large deduplication to declare: sized to deduplicate up to max_element_count keys, and named for
    -- debugging `<name>.keys`, `<name>.unique_keys`, and so on when name is set.
    record DedupLargeBuffersDescriptor
        name: string
        max_element_count: integer
    end

    -- The graph resources of a large deduplication.
    record DedupLargeBuffers
        -- The keys, sorted in place. The deduplication reads and writes it, so order its producers before and its
        -- consumers after the task dispatching the deduplication on it.
        keys: graph.Resource
        -- The unique keys in ascending order.
        unique_keys: graph.Resource
        -- Scratch buffers the deduplication alone uses.
        sort: sort.RadixSortBuffers
        marks: graph.Resource
        scan: large_prefix_scan.LargePrefixScanBuffers
        dispatch: graph.Resource
    end
end

local function member_name(name: string, member: string): string
    return name ~= nil and name .. "." .. member or nil
end

-- Graph-owned buffers of a large deduplication. The dispatches reference these handles, so the build
-- discovers them.
function dedup_large.buffers(descriptor: dedup_large.DedupLargeBuffersDescriptor): dedup_large.DedupLargeBuffers
    local max_element_count = descriptor.max_element_count
    local name = descriptor.name
    local sort_buffers = sort.radix_sort_buffers({ name = name, max_element_count = max_element_count })
    return {
        keys = sort_buffers.keys,
        -- At most every key is unique.
        unique_keys = graph.storage_buffer({
            name = member_name(name, "unique_keys"),
            size = large_prefix_scan.data_buffer_size(max_element_count),
        }),
        sort = sort_buffers,
        marks = graph.storage_buffer({
            name = member_name(name, "marks"),
            size = large_prefix_scan.data_buffer_size(max_element_count),
        }),
        scan = large_prefix_scan.buffers({ name = member_name(name, "mark_scan"), max_element_count = max_element_count }),
        dispatch = graph.buffer({
            name = member_name(name, "dedup_dispatch"),
            usage = "buffer_indirect",
            size = DEDUP_LARGE_DISPATCH_BUFFER_SIZE,
        }),
    }
end

-- The dispatches that sort buffers.keys in place and write its unique keys to buffers.unique_keys, bound to the
-- handles of buffers. Each pass follows the previous one, and the passes read the key count from the keys, so the count
-- must be written before they run and must not exceed the maximum element count buffers was sized for.
function dedup_large.dispatches(buffers: dedup_large.DedupLargeBuffers): {graph.RTParamsComputeDispatchVK}
    local keys = buffers.keys
    local marks = buffers.marks
    local dispatch = buffers.dispatch
    local dispatches: {graph.RTParamsComputeDispatchVK} = {
        { pipeline = graph.compute_pipeline("dedup_large_prepare_dispatch"),
            bindings = { keys = keys, dispatch_buffer = dispatch } },
    }
    local function append(output_dispatches: {graph.RTParamsComputeDispatchVK})
        for _, entry in ipairs(output_dispatches) do
            dispatches[#dispatches + 1] = entry
        end
    end
    append(sort.radix_sort_dispatches(buffers.sort))
    dispatches[#dispatches + 1] =
        graph.indirect_dispatch(graph.compute_pipeline("dedup_large_mark"), dispatch, 0, { keys = keys, marks = marks })
    append(large_prefix_scan.dispatches(marks, buffers.scan))
    dispatches[#dispatches + 1] = graph.indirect_dispatch(graph.compute_pipeline("dedup_large_scatter"), dispatch,
        0, { keys = keys, marks = marks, unique_keys = buffers.unique_keys })
    return dispatches
end

return dedup_large
