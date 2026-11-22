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
-- Render graph definitions. Each names its recipe, a `script` resource whose module exports `build(context, parameters)`,
-- and the recipe's static `parameters`; applications acquire instances of a definition with their runtime bindings
-- (IRenderer::acquire_rend_graph()).
return {
    {
        id = "draw_triangle_graph",
        type = "rend_graph",
        recipe = "rend_graphs.draw_triangle",
    },
    {
        id = "draw_cube_graph",
        type = "rend_graph",
        recipe = "rend_graphs.draw_cube",
    },
    {
        id = "draw_cube_unshaded_graph",
        type = "rend_graph",
        recipe = "rend_graphs.draw_cube_unshaded",
    },
    {
        id = "draw_material_variants_graph",
        type = "rend_graph",
        recipe = "rend_graphs.draw_material_variants",
    },
    {
        id = "draw_sponza_graph",
        type = "rend_graph",
        recipe = "rend_graphs.draw_sponza",
    },
    {
        id = "draw_skinned_mesh_graph",
        type = "rend_graph",
        recipe = "rend_graphs.draw_skinned_mesh",
    },
    {
        id = "pixel_sorting_graph",
        type = "rend_graph",
        recipe = "rend_graphs.pixel_sorting",
    },
    -- The deferred test's two graphs differ only in how they filter shadows.
    {
        id = "deferred_mipmap_graph",
        type = "rend_graph",
        recipe = "rend_graphs.deferred",
        parameters = { summed_area_table = false, bloom_blur_pass_count = 4 },
    },
    {
        id = "deferred_summed_area_table_graph",
        type = "rend_graph",
        recipe = "rend_graphs.deferred",
        parameters = { summed_area_table = true, bloom_blur_pass_count = 4 },
    },
}
