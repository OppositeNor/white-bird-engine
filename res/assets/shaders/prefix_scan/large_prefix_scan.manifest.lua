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
-- The default variant scans uint with Add; the segmented one scans SegmentedWrapper<float3x3> with SegmentedPrefix.
local function scan_variants()
    return {
        { name = "default" },
        {
            name = "segmented",
            values = {
                { name = "WBE_PREFIX_SCAN_TYPE", value = "SegmentedWrapper<float3x3>" },
                { name = "WBE_PREFIX_SCAN_OPERATION", value = "SegmentedPrefix<float3x3, Add<float3x3>>" },
            },
        },
    }
end

local resources = {
    {
        id = "large_prefix_scan_prepare_dispatch",
        source_type = "shader",
        src = "assets/shaders/prefix_scan/large_prefix_scan_prepare_dispatch.slang",
        variants = scan_variants(),
    },
    {
        id = "large_prefix_scan_blocks",
        source_type = "shader",
        src = "assets/shaders/prefix_scan/large_prefix_scan_blocks.slang",
        variants = scan_variants(),
    },
    {
        id = "large_prefix_scan_sections",
        source_type = "shader",
        src = "assets/shaders/prefix_scan/large_prefix_scan_sections.slang",
        variants = scan_variants(),
    },
    {
        id = "large_prefix_scan_totals",
        source_type = "shader",
        src = "assets/shaders/prefix_scan/large_prefix_scan_totals.slang",
        variants = scan_variants(),
    },
    {
        id = "large_prefix_scan_add",
        source_type = "shader",
        src = "assets/shaders/prefix_scan/large_prefix_scan_add.slang",
        variants = scan_variants(),
    },
}
return { resources = resources }
