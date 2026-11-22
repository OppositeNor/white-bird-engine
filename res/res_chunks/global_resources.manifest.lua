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

require("assets.meshes.cube")
require("assets.shaders.main_pipeline.main_pipeline_shader")
require("assets.shaders.forward_cook_torrance_pbr.forward_cook_torrance_pbr_shader")
require("assets.shaders.shadowmap_spot.shadowmap_spot_shader")
require("assets.shaders.kawase_blur.kawase_blur_shader")
require("assets.shaders.max_downsample.max_downsample_shader")
require("assets.shaders.svogi.svogi")
require("assets.materials.default_material")
require("res_chunks.test_global_resources")

return {}
