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

-- GPU inclusive prefix sum of large buffers for render graph recipes, as the dispatches of a
-- run_compute_pipelines_sequential task, which may dispatch other passes before and after them: each 256-element block
-- is scanned and its total stored, 256 sections of the block totals are scanned and their totals stored, the section
-- totals are scanned in one workgroup, and every block adds the sum of the blocks before it. The application compiles the pipelines under their shader ids (large_prefix_scan_prepare_dispatch,
-- large_prefix_scan_blocks, large_prefix_scan_sections, large_prefix_scan_totals, and large_prefix_scan_add). The
-- scanned buffer holds the element count in its metadata, followed by the elements; see large_prefix_scan_utils.slang.

local math_utils = require("utils.math")
local graph = require("render_graph_utils.render_graph_utils")

local ceil_div = math_utils.ceil_div

-- Must match WBE_LARGE_PREFIX_SCAN_BLOCK_SIZE, WBE_LARGE_PREFIX_SCAN_SECTION_NUM, and WBE_LARGE_PREFIX_SCAN_DATA_START in
-- large_prefix_scan_utils.slang.
local LARGE_PREFIX_SCAN_BLOCK_SIZE = 256
local LARGE_PREFIX_SCAN_SECTION_NUM = 256
local LARGE_PREFIX_SCAN_DATA_START = 4
-- The block passes dispatch one workgroup per block, within the maxComputeWorkGroupCount every device supports.
local LARGE_PREFIX_SCAN_MAX_BLOCK_NUM = 65535
-- One VkDispatchIndirectCommand (3 uint32), the block pass dispatch.
local LARGE_PREFIX_SCAN_DISPATCH_BUFFER_SIZE = 12
local UINT32_SIZE = 4

local record large_prefix_scan
    -- The scratch buffers of a large prefix scan to declare: sized to scan up to max_element_count elements, and named for
    -- debugging `<name>.sections` and `<name>.dispatch` when name is set.
    record LargePrefixScanBuffersDescriptor
        name: string
        max_element_count: integer
    end

    -- The scratch buffers of a large prefix scan, which the scan alone uses: the block and section totals, and the
    -- indirect dispatch of the block passes.
    record LargePrefixScanBuffers
        sections: graph.Resource
        dispatch: graph.Resource
    end
end

-- Graph-owned scratch buffers of a large prefix scan. The scan dispatches reference these handles, so the build
-- discovers them.
function large_prefix_scan.buffers(descriptor: large_prefix_scan.LargePrefixScanBuffersDescriptor): large_prefix_scan.LargePrefixScanBuffers
    local max_element_count = descriptor.max_element_count
    local name = descriptor.name
    local max_block_num = ceil_div(max_element_count, LARGE_PREFIX_SCAN_BLOCK_SIZE)
    if max_block_num > LARGE_PREFIX_SCAN_MAX_BLOCK_NUM then
        error(string.format("large_prefix_scan: %d elements need more than %d blocks.", max_element_count,
            LARGE_PREFIX_SCAN_MAX_BLOCK_NUM))
    end
    return {
        -- The section totals, then one block total per block.
        sections = graph.storage_buffer({
            name = name ~= nil and name .. ".sections" or nil,
            size = (LARGE_PREFIX_SCAN_SECTION_NUM + math.max(max_block_num, 1)) * UINT32_SIZE,
        }),
        dispatch = graph.buffer({
            name = name ~= nil and name .. ".dispatch" or nil,
            usage = "buffer_indirect",
            size = LARGE_PREFIX_SCAN_DISPATCH_BUFFER_SIZE,
        }),
    }
end

-- The size in bytes of a buffer scanned by a large prefix scan of up to max_element_count elements.
function large_prefix_scan.data_buffer_size(max_element_count: integer): integer
    return (LARGE_PREFIX_SCAN_DATA_START + max_element_count) * UINT32_SIZE
end

-- The dispatches that compute the inclusive prefix sum of data in place, bound to the handles of data and buffers.
-- Each pass follows the previous one, and the passes read the element count from data, so the count must be written
-- before they run and must not exceed the maximum element count buffers was sized for.
function large_prefix_scan.dispatches(data: graph.Resource,
    buffers: large_prefix_scan.LargePrefixScanBuffers): {graph.RTParamsComputeDispatchVK}
    local sections = buffers.sections
    local dispatch = buffers.dispatch
    return {
        { pipeline = graph.compute_pipeline("large_prefix_scan_prepare_dispatch"),
            bindings = { data_buffer = data, dispatch_buffer = dispatch } },
        graph.indirect_dispatch(graph.compute_pipeline("large_prefix_scan_blocks"), dispatch, 0,
            { data_buffer = data, sections_buffer = sections }),
        { pipeline = graph.compute_pipeline("large_prefix_scan_sections"),
            group_count_x = LARGE_PREFIX_SCAN_SECTION_NUM, bindings = { data_buffer = data, sections_buffer = sections } },
        { pipeline = graph.compute_pipeline("large_prefix_scan_totals"), bindings = { sections_buffer = sections } },
        graph.indirect_dispatch(graph.compute_pipeline("large_prefix_scan_add"), dispatch, 0,
            { data_buffer = data, sections_buffer = sections }),
    }
end

return large_prefix_scan
