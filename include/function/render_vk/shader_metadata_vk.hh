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
#ifndef WBE_FILE_SHADER_METADATA_VK_HH
#define WBE_FILE_SHADER_METADATA_VK_HH

#include "function/render/shader_metadata.hh"
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @brief Vulkan-friendly entry point metadata.
 */
struct ShaderEntryPointVK {
    std::string name;
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
};

/**
 * @brief Vulkan-friendly descriptor binding metadata.
 */
struct ShaderDescriptorBindingVK {
    std::string name;
    VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uint32_t set = 0;
    uint32_t binding = 0;
    uint32_t count = 1;
    VkShaderStageFlags stage_flags = VK_SHADER_STAGE_ALL;
};

/**
 * @brief Vulkan-focused view of ShaderMetadata for pipeline and layout creation.
 */
struct ShaderMetadataVK {
    std::vector<ShaderEntryPointVK> entry_points;
    std::vector<ShaderDescriptorBindingVK> descriptor_bindings;
    int32_t bindless_space_index = -1;

    /**
     * @brief Build Vulkan-friendly metadata from reflected shader metadata.
     */
    static ShaderMetadataVK from_shader_metadata(const ShaderMetadata& p_metadata) {
        ShaderMetadataVK result;
        result.bindless_space_index = p_metadata.bindlessSpaceIndex;

        for (const auto& entry_point : p_metadata.entryPoints) {
            result.entry_points.push_back({
                .name = entry_point.name,
                .stage = shader_stage_from_str(entry_point.stage)
            });

            for (const auto& binding : entry_point.bindings) {
                if (binding.index < 0) {
                    continue;
                }
                ShaderDescriptorBindingVK descriptor_binding{};
                descriptor_binding.name = entry_point.name;
                descriptor_binding.descriptor_type = descriptor_type_from_str(binding.kind);
                descriptor_binding.set = binding.space < 0 ? 0U : static_cast<uint32_t>(binding.space);
                descriptor_binding.binding = static_cast<uint32_t>(binding.index);
                descriptor_binding.count = binding.count <= 0 ? 1U : static_cast<uint32_t>(binding.count);
                descriptor_binding.stage_flags = shader_stage_flags_from_str(entry_point.stage);
                merge_descriptor_binding(result.descriptor_bindings, descriptor_binding);
            }
        }

        for (const auto& parameter : p_metadata.parameters) {
            if (parameter.binding.index < 0) {
                continue;
            }
            ShaderDescriptorBindingVK descriptor_binding{};
            descriptor_binding.name = parameter.name;
            descriptor_binding.descriptor_type = descriptor_type_from_str(parameter.binding.kind);
            descriptor_binding.set = parameter.binding.space < 0 ? 0U : static_cast<uint32_t>(parameter.binding.space);
            descriptor_binding.binding = static_cast<uint32_t>(parameter.binding.index);
            descriptor_binding.count = parameter.binding.count <= 0 ? 1U : static_cast<uint32_t>(parameter.binding.count);
            descriptor_binding.stage_flags = shader_stage_flags_from_str(parameter.stage);
            merge_descriptor_binding(result.descriptor_bindings, descriptor_binding);
        }

        return result;
    }

    /**
     * @brief Get descriptor set layout bindings for a specific descriptor set index.
     */
    std::vector<VkDescriptorSetLayoutBinding> get_descriptor_set_layout_bindings(uint32_t p_set_index) const {
        std::vector<VkDescriptorSetLayoutBinding> result;
        for (const auto& binding : descriptor_bindings) {
            if (binding.set != p_set_index) {
                continue;
            }
            result.push_back({
                .binding = binding.binding,
                .descriptorType = binding.descriptor_type,
                .descriptorCount = binding.count,
                .stageFlags = binding.stage_flags,
                .pImmutableSamplers = nullptr
            });
        }
        return result;
    }

private:
    static std::string normalize_key(std::string p_str) {
        std::string result;
        result.reserve(p_str.size());
        for (char ch : p_str) {
            if (std::isalnum(static_cast<unsigned char>(ch)) == 0) {
                continue;
            }
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return result;
    }

    static VkShaderStageFlagBits shader_stage_from_str(const std::string& p_stage) {
        const std::string key = normalize_key(p_stage);
        if (key == "vertex") {
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (key == "fragment" || key == "pixel") {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (key == "compute") {
            return VK_SHADER_STAGE_COMPUTE_BIT;
        }
        if (key == "geometry") {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        if (key == "tesscontrol" || key == "hull") {
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        }
        if (key == "tesseval" || key == "domain") {
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        throw std::runtime_error("Unsupported shader stage in metadata: " + p_stage);
    }

    static VkShaderStageFlags shader_stage_flags_from_str(const std::string& p_stage) {
        return static_cast<VkShaderStageFlags>(shader_stage_from_str(p_stage));
    }

    static VkDescriptorType descriptor_type_from_str(const std::string& p_kind) {
        const std::string key = normalize_key(p_kind);
        if (key == "constantbuffer" || key == "uniformbuffer" || key == "cbuffer") {
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        if (key == "structuredbuffer" || key == "storagebuffer" || key == "rwstructuredbuffer" || key == "byteaddressbuffer") {
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        if (key == "sampler") {
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        }
        if (key == "texture" || key == "texture2d" || key == "texturecube" || key == "sampledimage") {
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }
        if (key == "sampler2d" || key == "samplercube" || key == "combinedimagesampler") {
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        if (key == "rwtexture" || key == "rwtexture2d" || key == "storageimage") {
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        throw std::runtime_error("Unsupported descriptor kind in metadata: " + p_kind);
    }

    static void merge_descriptor_binding(
        std::vector<ShaderDescriptorBindingVK>& p_bindings,
        const ShaderDescriptorBindingVK& p_binding) {
        for (auto& existing : p_bindings) {
            if (existing.set != p_binding.set || existing.binding != p_binding.binding
                || existing.descriptor_type != p_binding.descriptor_type || existing.count != p_binding.count) {
                continue;
            }
            existing.stage_flags |= p_binding.stage_flags;
            return;
        }
        p_bindings.push_back(p_binding);
    }
};

} // namespace WhiteBirdEngine

#endif
