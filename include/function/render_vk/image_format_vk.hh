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
#ifndef WBE_FILE_IMAGE_FORMAT_VK_HH
#define WBE_FILE_IMAGE_FORMAT_VK_HH

#include "core/reflection/reflection_defs.hh"
#include <type_traits>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @enum ImageFormatVK
 * @brief Reflected subset of VkFormat for render task parameters.
 *
 * Each enumerator has the value of the VkFormat it names; the aliases are the lowercase names. Add an enumerator here to
 * make another format available to render graph recipes.
 */
enum class WBE_META(WBE_REFLECT_ENUM) ImageFormatVK : std::underlying_type_t<VkFormat> {
    UNDEFINED WBE_META(WBE_ALIASES = [undefined]) = VK_FORMAT_UNDEFINED,
    R8_UNORM WBE_META(WBE_ALIASES = [r8_unorm]) = VK_FORMAT_R8_UNORM,
    R8G8_UNORM WBE_META(WBE_ALIASES = [r8g8_unorm]) = VK_FORMAT_R8G8_UNORM,
    R8G8B8A8_UNORM WBE_META(WBE_ALIASES = [r8g8b8a8_unorm]) = VK_FORMAT_R8G8B8A8_UNORM,
    R8G8B8A8_SRGB WBE_META(WBE_ALIASES = [r8g8b8a8_srgb]) = VK_FORMAT_R8G8B8A8_SRGB,
    B8G8R8A8_UNORM WBE_META(WBE_ALIASES = [b8g8r8a8_unorm]) = VK_FORMAT_B8G8R8A8_UNORM,
    B8G8R8A8_SRGB WBE_META(WBE_ALIASES = [b8g8r8a8_srgb]) = VK_FORMAT_B8G8R8A8_SRGB,
    A2B10G10R10_UNORM_PACK32 WBE_META(WBE_ALIASES = [a2b10g10r10_unorm_pack32]) = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    B10G11R11_UFLOAT_PACK32 WBE_META(WBE_ALIASES = [b10g11r11_ufloat_pack32]) = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
    R16_SFLOAT WBE_META(WBE_ALIASES = [r16_sfloat]) = VK_FORMAT_R16_SFLOAT,
    R16G16_SFLOAT WBE_META(WBE_ALIASES = [r16g16_sfloat]) = VK_FORMAT_R16G16_SFLOAT,
    R16G16B16A16_SFLOAT WBE_META(WBE_ALIASES = [r16g16b16a16_sfloat]) = VK_FORMAT_R16G16B16A16_SFLOAT,
    R16_UINT WBE_META(WBE_ALIASES = [r16_uint]) = VK_FORMAT_R16_UINT,
    R32_SFLOAT WBE_META(WBE_ALIASES = [r32_sfloat]) = VK_FORMAT_R32_SFLOAT,
    R32G32_SFLOAT WBE_META(WBE_ALIASES = [r32g32_sfloat]) = VK_FORMAT_R32G32_SFLOAT,
    R32G32B32_SFLOAT WBE_META(WBE_ALIASES = [r32g32b32_sfloat]) = VK_FORMAT_R32G32B32_SFLOAT,
    R32G32B32A32_SFLOAT WBE_META(WBE_ALIASES = [r32g32b32a32_sfloat]) = VK_FORMAT_R32G32B32A32_SFLOAT,
    R32_UINT WBE_META(WBE_ALIASES = [r32_uint]) = VK_FORMAT_R32_UINT,
    R32G32_UINT WBE_META(WBE_ALIASES = [r32g32_uint]) = VK_FORMAT_R32G32_UINT,
    R32G32B32_UINT WBE_META(WBE_ALIASES = [r32g32b32_uint]) = VK_FORMAT_R32G32B32_UINT,
    R32G32B32A32_UINT WBE_META(WBE_ALIASES = [r32g32b32a32_uint]) = VK_FORMAT_R32G32B32A32_UINT,
    R32_SINT WBE_META(WBE_ALIASES = [r32_sint]) = VK_FORMAT_R32_SINT,
    D16_UNORM WBE_META(WBE_ALIASES = [d16_unorm]) = VK_FORMAT_D16_UNORM,
    D32_SFLOAT WBE_META(WBE_ALIASES = [d32_sfloat]) = VK_FORMAT_D32_SFLOAT,
    D24_UNORM_S8_UINT WBE_META(WBE_ALIASES = [d24_unorm_s8_uint]) = VK_FORMAT_D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT WBE_META(WBE_ALIASES = [d32_sfloat_s8_uint]) = VK_FORMAT_D32_SFLOAT_S8_UINT,
};

/**
 * @brief Convert to the VkFormat with the same value.
 */
constexpr VkFormat to_vk_format(ImageFormatVK p_format) {
    return static_cast<VkFormat>(p_format);
}

} // namespace WhiteBirdEngine

#endif
