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
return {
    {
        id = "copy_pixel_sort_source_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/copy_pixel_sort_source.slang",
    },
    {
        id = "count_pixel_sort_mask_vertical_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/count_pixel_sort_mask.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_PIXEL_SORT_HORIZONTAL", value = "0" } },
            },
        },
    },
    {
        id = "count_pixel_sort_mask_horizontal_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/count_pixel_sort_mask.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_PIXEL_SORT_HORIZONTAL", value = "1" } },
            },
        },
    },
    {
        id = "prefix_sum_pixel_sort_columns_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/prefix_sum_pixel_sort_columns.slang",
    },
    {
        id = "register_pixel_sort_keys_vertical_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/register_pixel_sort_keys.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_PIXEL_SORT_HORIZONTAL", value = "0" } },
            },
        },
    },
    {
        id = "register_pixel_sort_keys_horizontal_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/register_pixel_sort_keys.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_PIXEL_SORT_HORIZONTAL", value = "1" } },
            },
        },
    },
    {
        id = "write_sorted_pixels_vertical_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/write_sorted_pixels.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_PIXEL_SORT_HORIZONTAL", value = "0" } },
            },
        },
    },
    {
        id = "write_sorted_pixels_horizontal_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/write_sorted_pixels.slang",
        variants = {
            {
                name = "default",
                values = { { name = "WBE_PIXEL_SORT_HORIZONTAL", value = "1" } },
            },
        },
    },
    {
        id = "pixel_sorting_post_processing_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/pixel_sorting_post_processing.slang",
    },
    {
        id = "present_pixel_sort_shader",
        source_type = "shader",
        src = "assets/shaders/pixel_sort/present_pixel_sort.slang",
    },
    {
        id = "present_pixel_sort_pipeline",
        type = "graphics_pipeline",
        vertex_stage_id = "present_pixel_sort_shader",
        fragment_stage_id = "present_pixel_sort_shader",
        depth_test = false,
        depth_write = false,
    },
}
