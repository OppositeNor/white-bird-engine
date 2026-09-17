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
#include "function/render_vk/static_set_buffer_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_database.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

struct StaticSetDrawBinaries {
    RefRes<RUBinary> vertex_binary = MEM_NULL;
    RefRes<RUBinary> index_binary = MEM_NULL;
};

bool is_drawable_submesh(const RUStaticGeometrySubmesh& p_submesh) {
    return p_submesh.indices.index_count > 0 && p_submesh.instance_count > 0;
}

void validate_binary_view(const RUBinaryView& p_view, std::string_view p_context) {
    if (p_view.binary == nullptr) {
        throw std::runtime_error(std::format("Failed to compile static set buffer: {} binary is missing.", p_context));
    }
    if (p_view.start > p_view.binary->size || p_view.size > p_view.binary->size - p_view.start) {
        throw std::runtime_error(std::format("Failed to compile static set buffer: {} binary view exceeds binary size.", p_context));
    }
    if (p_view.size > 0 && p_view.binary->data == nullptr) {
        throw std::runtime_error(std::format("Failed to compile static set buffer: {} binary data is missing.", p_context));
    }
}

StaticSetDrawBinaries validate_static_set_and_get_binaries(const RUStaticGeometrySet& p_set) {
    if (p_set.submesh_count > 0 && p_set.submeshes == nullptr) {
        throw std::runtime_error("Failed to compile static set buffer: submesh storage is missing.");
    }
    if (p_set.instance_count > 0 && p_set.instances == nullptr) {
        throw std::runtime_error("Failed to compile static set buffer: instance storage is missing.");
    }
    StaticSetDrawBinaries binaries;
    for (size_t i = 0; i < p_set.submesh_count; ++i) {
        const RUStaticGeometrySubmesh& submesh = p_set.submeshes.get()[i];
        validate_binary_view(submesh.vertices.binary, "vertex");
        validate_binary_view(submesh.indices.binary, "index");
        if (submesh.vertices.vertex_stride == 0 ||
            submesh.vertices.binary.size != submesh.vertices.vertex_count * submesh.vertices.vertex_stride) {
            throw std::runtime_error("Failed to compile static set buffer: vertex view count, stride, and size do not match.");
        }
        if (submesh.indices.binary.start % sizeof(uint32_t) != 0 ||
            submesh.indices.binary.size != submesh.indices.index_count * sizeof(uint32_t)) {
            throw std::runtime_error("Failed to compile static set buffer: index view is not a uint32 range.");
        }
        if (submesh.first_instance > p_set.instance_count || submesh.instance_count > p_set.instance_count - submesh.first_instance) {
            throw std::runtime_error("Failed to compile static set buffer: submesh instance range is out of bounds.");
        }
        if (!is_drawable_submesh(submesh)) {
            continue;
        }
        if (binaries.vertex_binary == nullptr) {
            binaries.vertex_binary = submesh.vertices.binary.binary;
        }
        if (binaries.index_binary == nullptr) {
            binaries.index_binary = submesh.indices.binary.binary;
        }
        if (submesh.vertices.binary.binary != binaries.vertex_binary) {
            throw std::runtime_error("Failed to compile static set buffer: all drawable submeshes must share one vertex binary resource.");
        }
        if (submesh.indices.binary.binary != binaries.index_binary) {
            throw std::runtime_error("Failed to compile static set buffer: all drawable submeshes must share one index binary resource.");
        }
    }
    return binaries;
}

std::string build_buffer_cache_key(RefRes<RUBinary> p_binary, VkBufferUsageFlags p_usage) {
    if (p_binary == nullptr) {
        throw std::runtime_error("Failed to build static set buffer cache key: binary resource is missing.");
    }
    if (p_binary->resource_key != nullptr && p_binary->resource_key[0] != '\0') {
        return std::format("{}#vk_usage_{}", p_binary->resource_key, static_cast<uint64_t>(p_usage));
    }
    return std::format("binary_resource_{}#vk_usage_{}", reinterpret_cast<uintptr_t>(p_binary.get()), static_cast<uint64_t>(p_usage));
}

Ref<ROBufferVK> build_static_binary_buffer(
    const BaseRenderDataVK* p_render_data, RefRes<RUBinary> p_binary, VkBufferUsageFlags p_usage, std::string_view p_resource_kind) {
    if (p_render_data == nullptr || p_render_data->render_database == nullptr) {
        throw std::runtime_error("Failed to compile static set buffer: render database is missing.");
    }
    if (p_binary == nullptr || p_binary->data == nullptr || p_binary->size == 0) {
        throw std::runtime_error(std::format("Failed to compile static set buffer: {} binary data is missing.", p_resource_kind));
    }

    const std::string cache_key = build_buffer_cache_key(p_binary, p_usage);
    Ref<IRenderObject> registered_buffer =
        p_render_data->render_database->get_compiled_render_resource(cache_key, TypeUUID<ROBufferVK>::UUID);
    if (registered_buffer != MEM_NULL) {
        Ref<ROBufferVK> result = registered_buffer.dynamic_cast_ref<ROBufferVK>();
        if (result == MEM_NULL) {
            throw std::runtime_error("Failed to compile static set buffer: registered geometry buffer has the wrong type.");
        }
        if (result->fields().buffer_size != p_binary->size) {
            throw std::runtime_error(
                "Failed to compile static set buffer: registered geometry buffer size does not match its binary resource.");
        }
        return result;
    }

    Ref<ROBufferVK> result =
        ROBufferVK::Builder().set_domain(DeviceDomain::HOST_DEVICE).set_size(p_binary->size).set_usage(p_usage).create_ref(p_render_data);
    result->update_buffer(p_binary->data.get(), p_binary->size);
    p_render_data->render_database->register_compiled_render_resource(cache_key, result);
    return result;
}

} // namespace

StaticSetBufferCompilerVK::StaticSetBufferCompilerVK(const BaseRenderDataVK* p_render_data) : render_data(p_render_data) {
    if (render_data == nullptr || render_data->allocator == nullptr || render_data->render_database == nullptr) {
        throw std::runtime_error("Failed to create static set buffer compiler: renderer state is missing.");
    }
}

StaticSetBufferCompilerVK::BuildResult StaticSetBufferCompilerVK::build(
    RefRes<RUStaticGeometrySet> p_set, VkBufferUsageFlags p_vertex_usage, VkBufferUsageFlags p_index_usage) const {
    if (p_set == nullptr) {
        return {};
    }
    const StaticSetDrawBinaries binaries = validate_static_set_and_get_binaries(*p_set);
    if (binaries.vertex_binary == nullptr || binaries.index_binary == nullptr) {
        return {};
    }
    BuildResult result;
    result.vertex_buffer = build_static_binary_buffer(render_data, binaries.vertex_binary, p_vertex_usage, "vertex");
    result.index_buffer = build_static_binary_buffer(render_data, binaries.index_binary, p_index_usage, "index");
    return result;
}

} // namespace WhiteBirdEngine