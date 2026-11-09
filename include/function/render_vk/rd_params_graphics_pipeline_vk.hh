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
#ifndef WBE_FILE_RD_PARAMS_GRAPHICS_PIPELINE_VK_HH
#define WBE_FILE_RD_PARAMS_GRAPHICS_PIPELINE_VK_HH

#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class RDParamsGraphicsPipelineVK
 * @brief Description of a graphics pipeline registered on a render graph under the "graphics_pipeline" kind
 * (RRGraphicsPipelineVK). Copyable and movable.
 *
 * Graphics tasks name the pipeline by its registration key. Deserialization is strict.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT) RDParamsGraphicsPipelineVK {
    /**
     * @brief ResourceManager key of the RUGraphicsPipeline resource.
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string pipeline;
    /**
     * @brief Dynamic-rendering color attachment formats, one per render target.
     */
    WBE_META(WBE_REFLECT)
    std::vector<ImageFormatVK> color_formats;
    /**
     * @brief Dynamic-rendering depth attachment format; UNDEFINED for none.
     */
    WBE_META(WBE_REFLECT)
    ImageFormatVK depth_format = ImageFormatVK::UNDEFINED;
    /**
     * @brief Dynamic-rendering stencil attachment format; UNDEFINED for none.
     */
    WBE_META(WBE_REFLECT)
    ImageFormatVK stencil_format = ImageFormatVK::UNDEFINED;
    /**
     * @brief Primitive topology the pipeline assembles its vertices into.
     */
    WBE_META(WBE_REFLECT)
    PrimitiveTopologyVK primitive_topology = PrimitiveTopologyVK::TRIANGLE_LIST;
    /**
     * @brief Polygon rasterization mode.
     */
    WBE_META(WBE_REFLECT)
    PolygonModeVK polygon_mode = PolygonModeVK::FILL;

    bool operator==(const RDParamsGraphicsPipelineVK&) const = default;
};

} // namespace WhiteBirdEngine

#endif
