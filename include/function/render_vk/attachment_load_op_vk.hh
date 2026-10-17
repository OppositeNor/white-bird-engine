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
#ifndef WBE_FILE_ATTACHMENT_LOAD_OP_VK_HH
#define WBE_FILE_ATTACHMENT_LOAD_OP_VK_HH

#include "core/reflection/reflection_defs.hh"
#include <type_traits>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @enum AttachmentLoadOpVK
 * @brief Reflected mirror of VkAttachmentLoadOp for render task parameters.
 *
 * Each enumerator has the value of the VkAttachmentLoadOp it names; the lowercase aliases are the names used in render graph
 * recipes.
 */
enum class WBE_META(WBE_REFLECT_ENUM) AttachmentLoadOpVK : std::underlying_type_t<VkAttachmentLoadOp> {
    LOAD WBE_META(WBE_ALIASES = [load]) = VK_ATTACHMENT_LOAD_OP_LOAD,
    CLEAR WBE_META(WBE_ALIASES = [clear]) = VK_ATTACHMENT_LOAD_OP_CLEAR,
    DONT_CARE WBE_META(WBE_ALIASES = [dont_care]) = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
};

/**
 * @brief Convert to the VkAttachmentLoadOp with the same value.
 */
constexpr VkAttachmentLoadOp to_vk_attachment_load_op(AttachmentLoadOpVK p_load_op) {
    return static_cast<VkAttachmentLoadOp>(p_load_op);
}

} // namespace WhiteBirdEngine

#endif
