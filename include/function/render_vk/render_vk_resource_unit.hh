/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef WBE_FILE_RENDER_VK_RESOURCE_UNIT_HH
#define WBE_FILE_RENDER_VK_RESOURCE_UNIT_HH

#include "resource/resource_unit/resource_unit.hh"

namespace WhiteBirdEngine {

/**
 * @class RUShader
 * @brief Shader resource unit. contains a size (in bytes) and a MemID pointing to
 * the data chunk that stores the data of the binary data.
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 *
 */
struct RUShader : public ResourceUnit {
    // The binary data.
    RefRes<char> data;
    // The size of the binary data.
    size_t size;
    // The path to the metadata. Ending with an NUL terminater.
    RefRes<char> metadata_path;
};

/**
 * @class RUGraphicsPipeline
 * @brief Graphics pipeline.
 *
 */
struct RUGraphicsPipeline : public ResourceUnit {
    // The vertex shader.
    RefRes<RUShader> vertex_stage;
    // Entry function for the vertex stage.
    RefRes<char> vertex_stage_entry;
    // The tessellation shader.
    RefRes<RUShader> tessellation_stage;
    // Entry function for the tessellation stage.
    RefRes<char> tessellation_stage_entry;
    // The geometry shader.
    RefRes<RUShader> geometry_stage;
    // Entry function for the geometry stage.
    RefRes<char> geometry_stage_entry;
    // The fragment shader.
    RefRes<RUShader> fragment_stage;
    // Entry function for the fragment stage.
    RefRes<char> fragment_stage_entry;
    // Should enable depth test.
    bool depth_test = true;
    // Should enable depth write.
    bool depth_write = true;
};

/**
 * @class RUMaterial
 * @brief Material resource unit.
 */
struct RUMaterial : public ResourceUnit {
    int32_t placeholder;     // TODO
};

} // namespace WhiteBirdEngine

#endif
