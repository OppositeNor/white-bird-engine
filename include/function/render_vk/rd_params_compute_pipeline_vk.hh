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
#ifndef WBE_FILE_RD_PARAMS_COMPUTE_PIPELINE_VK_HH
#define WBE_FILE_RD_PARAMS_COMPUTE_PIPELINE_VK_HH

#include "core/reflection/reflection_defs.hh"
#include <string>

namespace WhiteBirdEngine {

/**
 * @class RDParamsComputePipelineVK
 * @brief Description of a compute pipeline registered on a render graph under the "compute_pipeline" kind
 * (RRComputePipelineVK). Copyable and movable.
 *
 * Compute tasks name the pipeline by its registration key. Deserialization is strict.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT) RDParamsComputePipelineVK {
    /**
     * @brief ResourceManager key of the RUShader resource.
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string shader;

    bool operator==(const RDParamsComputePipelineVK&) const = default;
};

} // namespace WhiteBirdEngine

#endif
