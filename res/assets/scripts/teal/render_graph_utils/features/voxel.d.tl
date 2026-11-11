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

-- Types shared by the voxel structure feature modules (render_graph_utils.features.voxel_svo and voxel_octree), and the
-- recipes that include them. Declarations only: import them with `local type voxel = require("render_graph_utils.features.voxel")`.

local type wbe = require("wbe")
local type graph = require("render_graph_utils.render_graph")

local record voxel
    -- What the including recipe supplies to a voxel structure's subgraph, whose ports `color` and `depth` the overlays draw
    -- through.
    record Target
        -- The whole-mesh transforms of the two dynamic mesh entities, members of their imports.
        dynamic_mesh_transforms: {graph.Resource}
        -- The voxel debug view toggles by mode group.
        debug_view_toggles: wbe.VoxelDebugViewToggles
        -- The color target of the overlays.
        color_target: graph.Resource
        -- The depth target the overlays test against.
        depth_target: graph.Resource
        -- The formats of the color and depth targets, which the overlay pipelines render to.
        color_format: wbe.ImageFormatVK
        depth_format: wbe.ImageFormatVK
        -- The lights' reflective-shadow-map slots, by the names of the lights[] array of the lighting shaders (for example
        -- "shadowmap_color[0]"); the structures that inject radiance bind them through ports of those names.
        rsm_slots: {string:graph.Resource}
        -- The sampler of the lights' shadow maps, which the radiance injection samples the slots with.
        shadow_sampler: graph.Resource
    end
end

return voxel
