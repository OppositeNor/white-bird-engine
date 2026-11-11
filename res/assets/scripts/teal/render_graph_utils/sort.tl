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

-- GPU sorting for render graph recipes. The radix sort sorts a key buffer of uint or uint2 keys in place, as one task that
-- dispatches every pass in one command buffer. The application compiles the radix sort pipelines under their shader ids
-- (radix_sort_prepare_dispatch, radix_sort_histogram_prefix_sum, radix_sort_histogram_overflow_prefix_sum,
-- radix_sort_histogram_add_overflow, and the per-pass radix_sort_histogram_workgroup_process_<bits> and
-- radix_sort_scatter_<bits>, or for uint2 keys radix_sort_uint2_histogram_workgroup_process_<bits> and
-- radix_sort_uint2_scatter_<bits>). The key buffer is a counted buffer: a header holding the byte size of the keys and
-- the local sort count, followed by the keys; see counted_buffer.slang, radix_sort_utils.slang, and
-- radix_sort_uint2_utils.slang. uint2 keys are ordered by x and then by y; sorting only bits
-- [32, 64), the bits of x, orders them by x and carries y as a payload.

local type wbe = require("wbe")
local math_utils = require("utils.math")
local graph = require("render_graph_utils.render_graph_utils")

local ceil_div = math_utils.ceil_div

-- Must match WBE_RADIX_SORT_BUCKET_COUNT, WBE_RADIX_SORT_ELEMENTS_PER_WORKGROUP, WBE_RADIX_SORT_MAX_WORKGROUP_COUNT,
-- WBE_RADIX_SORT_SCAN_HISTOGRAM_NUM, WBE_RADIX_SORT_LOCAL_SORT_DISPATCH_START, and
-- WBE_RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_START in radix_sort_utils.slang, and RADIX_SORT_DATA_START the header uints,
-- WBE_COUNTED_BUFFER_HEADER_WORD_NUM in counted_buffer.slang.
local RADIX_SORT_BUCKET_COUNT = 256
local RADIX_SORT_ELEMENTS_PER_WORKGROUP = 256
local RADIX_SORT_MAX_WORKGROUP_COUNT = 16384
local RADIX_SORT_DATA_START = 4
local RADIX_SORT_SCAN_HISTOGRAM_NUM = 16
local RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET = 0
-- One VkDispatchIndirectCommand (3 uint32) after the local sort dispatch.
local RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET = 12
local RADIX_SORT_DISPATCH_BUFFER_SIZE = 24
-- Each pass sorts by one 8-bit digit of the keys.
local RADIX_SORT_DIGIT_BIT_SIZE = 8
local UINT32_SIZE = 4

local record sort
    -- The type of the keys of a radix sort.
    enum RadixSortKeyType
        "uint"
        "uint2"
    end

    -- The buffers of a radix sort to declare: sized for up to max_element_count keys of key_type (uint by default), and
    -- named for debugging `<name>.keys`, `<name>.keys_swap`, `<name>.histogram`, and `<name>.dispatch` when name is set.
    record RadixSortBuffersDescriptor
        name: string
        max_element_count: integer
        key_type: RadixSortKeyType
    end

    -- The graph resources of a radix sort.
    record RadixSortBuffers
        -- The keys, sorted in place. The sort reads and writes it, so order its producers before and its consumers after
        -- the sort task on it.
        keys: graph.Resource
        -- Scratch buffers the sort alone uses: the keys between passes, the histograms, and the indirect dispatches.
        keys_swap: graph.Resource
        histogram: graph.Resource
        dispatch: graph.Resource
        -- The type of the keys the buffers hold.
        key_type: RadixSortKeyType
    end

    -- The bits a radix sort orders the keys by, [begin_bit, end_bit) of the key as an unsigned integer; uint2 keys are
    -- the 64-bit (x << 32) | y. Both ends must be multiples of 8, and the range must span an even number of 8-bit digits,
    -- so that the sorted keys end up in the key buffer. Each end defaults to the respective end of the key.
    interface RadixSortBits
        begin_bit: integer
        end_bit: integer
    end

    -- A sort task: its debug name, its queue family (graphics by default), and the bits it sorts by.
    record RadixSortOptions is RadixSortBits
        name: string
        queue_family: wbe.QueueFamilyVK
    end
end

local KEY_BIT_SIZES: {sort.RadixSortKeyType:integer} = { uint = 32, uint2 = 64 }
-- The prefix of the ids of the per-pass pipelines of each key type.
local PASS_PIPELINE_PREFIXES: {sort.RadixSortKeyType:string} = { uint = "radix_sort_", uint2 = "radix_sort_uint2_" }

local function member_name(name: string, member: string): string
    return name ~= nil and name .. "." .. member or nil
end

-- The graph-owned buffers of a radix sort. The sort task references these handles, so the build discovers them.
function sort.radix_sort_buffers(descriptor: sort.RadixSortBuffersDescriptor): sort.RadixSortBuffers
    local key_type: sort.RadixSortKeyType = descriptor.key_type or "uint"
    local max_element_count = descriptor.max_element_count
    local name = descriptor.name
    -- The sort has one local sort per RADIX_SORT_ELEMENTS_PER_WORKGROUP keys, up to RADIX_SORT_MAX_WORKGROUP_COUNT.
    local max_local_sort_num =
        math_utils.clamp(ceil_div(max_element_count, RADIX_SORT_ELEMENTS_PER_WORKGROUP), 1, RADIX_SORT_MAX_WORKGROUP_COUNT)
    local max_overflow_num = ceil_div(max_local_sort_num, RADIX_SORT_SCAN_HISTOGRAM_NUM)
    local key_size = KEY_BIT_SIZES[key_type] // 8
    local key_buffer_size = RADIX_SORT_DATA_START * UINT32_SIZE + max_element_count * key_size
    return {
        keys = graph.storage_buffer({ name = member_name(name, "keys"), size = key_buffer_size }),
        keys_swap = graph.storage_buffer({ name = member_name(name, "keys_swap"), size = key_buffer_size }),
        -- The metadata, one sub-histogram per local sort, and one overflow histogram per histogram prefix scan workgroup.
        histogram = graph.storage_buffer({
            name = member_name(name, "histogram"),
            size = (RADIX_SORT_DATA_START + (max_local_sort_num + max_overflow_num) * RADIX_SORT_BUCKET_COUNT) * UINT32_SIZE,
        }),
        -- The indirect dispatches of the passes, written from the local sort count of the keys.
        dispatch = graph.buffer({
            name = member_name(name, "dispatch"),
            usage = "buffer_indirect",
            size = RADIX_SORT_DISPATCH_BUFFER_SIZE,
        }),
        key_type = key_type,
    }
end

-- The bit offsets of the digits a sort of buffers with settings orders by, from the least significant one up.
local function radix_sort_bit_regions(buffers: sort.RadixSortBuffers, settings?: sort.RadixSortBits): {integer}
    local key_bit_size = KEY_BIT_SIZES[buffers.key_type]
    local options: sort.RadixSortBits = settings or {}
    local begin_bit = options.begin_bit or 0
    local end_bit = options.end_bit or key_bit_size
    if begin_bit % RADIX_SORT_DIGIT_BIT_SIZE ~= 0 then
        error(string.format("sort: the begin bit %d of a radix sort is not a multiple of %d.", begin_bit,
            RADIX_SORT_DIGIT_BIT_SIZE))
    end
    if end_bit % RADIX_SORT_DIGIT_BIT_SIZE ~= 0 then
        error(string.format("sort: the end bit %d of a radix sort is not a multiple of %d.", end_bit,
            RADIX_SORT_DIGIT_BIT_SIZE))
    end
    if begin_bit < 0 then
        error(string.format("sort: the begin bit %d of a radix sort is negative.", begin_bit))
    end
    if end_bit > key_bit_size then
        error(string.format("sort: the end bit %d of a radix sort is past the %d bits of %s keys.", end_bit, key_bit_size,
            buffers.key_type))
    end
    if begin_bit >= end_bit then
        error(string.format("sort: the radix sort bit range [%d, %d) is empty.", begin_bit, end_bit))
    end
    if ((end_bit - begin_bit) // RADIX_SORT_DIGIT_BIT_SIZE) % 2 ~= 0 then
        error(string.format("sort: the radix sort bit range [%d, %d) spans an odd number of %d-bit digits.", begin_bit,
            end_bit, RADIX_SORT_DIGIT_BIT_SIZE))
    end
    local bit_regions: {integer} = {}
    for bit_region = begin_bit, end_bit - 1, RADIX_SORT_DIGIT_BIT_SIZE do
        bit_regions[#bit_regions + 1] = bit_region
    end
    return bit_regions
end

local function radix_sort_params(buffers: sort.RadixSortBuffers,
    queue_family?: wbe.QueueFamilyVK, settings?: sort.RadixSortBits): graph.RTParamsRunComputePipelinesSequentialVK
    local pass_pipeline_prefix = PASS_PIPELINE_PREFIXES[buffers.key_type]
    local keys = buffers.keys
    local keys_swap = buffers.keys_swap
    local histogram = buffers.histogram
    local dispatch = buffers.dispatch
    -- The element count is fixed once the keys are registered, so every pass uses the same dispatches.
    local dispatches: {graph.RTParamsComputeDispatchVK} = {
        { pipeline = graph.shader_compute_pipeline("radix_sort_prepare_dispatch"), bindings = { input_buffer = keys, dispatch_buffer = dispatch } },
    }
    local function indirect(pipeline: string, offset: integer, bindings: {string:graph.Resource})
        dispatches[#dispatches + 1] =
            { pipeline = graph.shader_compute_pipeline(pipeline), indirect_buffer = dispatch, indirect_offset = offset, bindings = bindings }
    end
    for pass, bit_offset in ipairs(radix_sort_bit_regions(buffers, settings)) do
        -- Pass p reads the keys from one buffer and scatters them into the other, so after an even number of passes
        -- the sorted keys are back in the key buffer.
        local input_keys = pass % 2 == 1 and keys or keys_swap
        local output_keys = pass % 2 == 1 and keys_swap or keys
        local bit_region = string.format("%d_%d", bit_offset, bit_offset + RADIX_SORT_DIGIT_BIT_SIZE)
        indirect(pass_pipeline_prefix .. "histogram_workgroup_process_" .. bit_region, RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            { input_buffer = input_keys, histogram_buffer = histogram })
        indirect("radix_sort_histogram_prefix_sum", RADIX_SORT_HISTOGRAM_SCAN_DISPATCH_OFFSET, { histogram_buffer = histogram })
        -- The overflow histograms are scanned in a single workgroup.
        dispatches[#dispatches + 1] =
            { pipeline = graph.shader_compute_pipeline("radix_sort_histogram_overflow_prefix_sum"), bindings = { histogram_buffer = histogram } }
        indirect("radix_sort_histogram_add_overflow", RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET, { histogram_buffer = histogram })
        indirect(pass_pipeline_prefix .. "scatter_" .. bit_region, RADIX_SORT_LOCAL_SORT_DISPATCH_OFFSET,
            { input_buffer = input_keys, histogram_buffer = histogram, output_buffer = output_keys })
    end
    return { dispatches = dispatches, queue_family = queue_family }
end

-- The dispatches that sort buffers.keys in place by the bits of settings, bound to the buffers' handles, for a
-- run_compute_pipelines_sequential task that may dispatch other passes before and after them. Like the sort task, they
-- read the element count from the keys, so the count must be written before they run.
function sort.radix_sort_dispatches(buffers: sort.RadixSortBuffers, settings?: sort.RadixSortBits): {graph.RTParamsComputeDispatchVK}
    return radix_sort_params(buffers, nil, settings).dispatches
end

-- The task that sorts buffers.keys in place.
function sort.radix_sort_task(buffers: sort.RadixSortBuffers, settings?: sort.RadixSortOptions): graph.Task
    local options: sort.RadixSortOptions = settings or {}
    return graph.compute_sequential(radix_sort_params(buffers, options.queue_family, options), { name = options.name })
end

-- The task that sorts the uint2 keys of buffers by x alone, which carries each y as the payload of its x. Keys with
-- equal x keep their input order. It runs the last 4 passes of the full uint2 sort, those of bits [32, 64), so settings
-- set no bits.
function sort.radix_sort_key_value_task(buffers: sort.RadixSortBuffers, settings?: sort.RadixSortOptions): graph.Task
    local options: sort.RadixSortOptions = settings or {}
    if buffers.key_type ~= "uint2" then
        error(string.format("sort: the key-value radix sort %s requires uint2 keys, got %s keys.", tostring(options.name),
            tostring(buffers.key_type)))
    end
    if options.begin_bit ~= nil or options.end_bit ~= nil then
        error("sort: the key-value radix sort sorts bits [32, 64); it takes no bit range.")
    end
    return sort.radix_sort_task(buffers, { name = options.name, queue_family = options.queue_family, begin_bit = 32 })
end

return sort
