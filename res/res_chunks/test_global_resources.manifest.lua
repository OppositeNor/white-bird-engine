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

require("assets.meshes.triangle")
require("assets.images.test_images")
require("assets.audio.test_audio")
require("assets.shaders.billboard.billboard_shader")
require("assets.shaders.draw_triangle.draw_triangle_shader")
require("assets.shaders.draw_cube_unshaded.draw_cube_unshaded_shader")
require("assets.shaders.draw_skinned_mesh.draw_skinned_mesh_shader")
require("assets.shaders.deferred_gbuffer.deferred_gbuffer_shader")
require("assets.shaders.deferred_composition.deferred_composition_shader")
require("assets.shaders.svo.svo")
require("assets.shaders.svo_2.svo_2")
require("assets.shaders.svo_3.svo_3")
require("assets.shaders.octree.octree")
require("assets.shaders.bloom.bloom_shader")
require("assets.shaders.sort.sort")
require("assets.shaders.prefix_scan.large_prefix_scan")
require("assets.shaders.dedup.dedup")
require("assets.shaders.pixel_sort.pixel_sort_shader")
require("assets.scripts.teal.lua_scripts")
require("assets.rend_graphs.rend_graphs")

return {
    scope = "test_only",
    resources = {
        {
            id = "test_text",
            source_type = "text",
            source_file = "test_text_files/test_file_utf8.txt",
        },
        {
            id = "test_script",
            source_type = "script",
            source_file = "test_scripts/test_script.lua",
        },
    },
}
