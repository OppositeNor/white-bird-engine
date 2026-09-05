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
#include "function/render/render_database.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_role.hh"
#include "function/render/vertex_attribute_format_info.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace WhiteBirdEngine {
namespace {

struct DrawGroupMeshInfo {
    std::string_view graphics_pipeline_key;
    std::string_view material_key;
    RefRes<RUMaterial> material;
    const RUVertexView* vertex_data = nullptr;
    Vector<RUSubmesh3D*, RenderAllocator> submeshes;
};

const uint8_t* get_binary_view_data(const RUBinaryView& p_binary_view, std::string_view p_context) {
    if (p_binary_view.binary == nullptr) {
        throw std::runtime_error(std::format("Failed to set opaque triangles: {} binary is missing.", p_context));
    }
    if (p_binary_view.start > p_binary_view.binary->size || p_binary_view.size > p_binary_view.binary->size - p_binary_view.start) {
        throw std::runtime_error(std::format("Failed to set opaque triangles: {} binary view range exceeds binary size.", p_context));
    }
    if (p_binary_view.size > 0 && p_binary_view.binary->data == nullptr) {
        throw std::runtime_error(std::format("Failed to set opaque triangles: {} binary data is missing.", p_context));
    }
    return reinterpret_cast<const uint8_t*>(p_binary_view.binary->data.get()) + p_binary_view.start;
}

void validate_vertex_data(const RUVertexView& p_vertex_data) {
    if (p_vertex_data.vertex_stride == 0) {
        throw std::runtime_error("Failed to set opaque triangles: vertex stride is zero.");
    }
    if (p_vertex_data.vertex_count > std::numeric_limits<size_t>::max() / p_vertex_data.vertex_stride ||
        p_vertex_data.binary.size != p_vertex_data.vertex_count * p_vertex_data.vertex_stride) {
        throw std::runtime_error("Failed to set opaque triangles: vertex storage size does not match its count and stride.");
    }
    get_binary_view_data(p_vertex_data.binary, "vertex");
    if (p_vertex_data.attribute_count > 0 && p_vertex_data.attributes == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: vertex attribute metadata is missing.");
    }
    size_t expected_attribute_offset = 0;
    for (size_t i = 0; i < p_vertex_data.attribute_count; ++i) {
        const RUVertexView::Attribute& attribute = p_vertex_data.attributes.get()[i];
        const std::string_view role_name = attribute.role.get_name();
        if (attribute.size != VertexAttributeFormatInfo::get_size(attribute.format) || attribute.offset != expected_attribute_offset ||
            attribute.size > std::numeric_limits<size_t>::max() - expected_attribute_offset) {
            throw std::runtime_error(std::format(
                "Failed to set opaque triangles: vertex attribute '{}' does not fit its declared format and stride.", role_name));
        }
        for (size_t previous_index = 0; previous_index < i; ++previous_index) {
            const RUVertexView::Attribute& previous = p_vertex_data.attributes.get()[previous_index];
            if (previous.role.get_name() == role_name) {
                throw std::runtime_error(
                    std::format("Failed to set opaque triangles: vertex attribute role '{}' is duplicated.", role_name));
            }
            if (previous.role.hash == attribute.role.hash) {
                throw std::runtime_error(std::format(
                    "Failed to set opaque triangles: distinct vertex attribute roles collide at hash {}.", attribute.role.hash));
            }
        }
        expected_attribute_offset += attribute.size;
    }
    if (expected_attribute_offset != p_vertex_data.vertex_stride) {
        throw std::runtime_error("Failed to set opaque triangles: vertex attributes are not tightly packed to the declared stride.");
    }
}

void validate_index_data(const RUSubmesh3D& p_submesh) {
    if (p_submesh.indices.index_count > 0 && p_submesh.indices.binary.size == 0) {
        throw std::runtime_error("Failed to set opaque triangles: submesh index storage is missing.");
    }
    if (p_submesh.indices.binary.size != p_submesh.indices.index_count * sizeof(uint32_t)) {
        throw std::runtime_error("Failed to set opaque triangles: submesh index storage size does not match its count.");
    }
    if (p_submesh.indices.index_count % 3 != 0) {
        throw std::runtime_error("Failed to set opaque triangles: submesh index count is not a multiple of three.");
    }
    get_binary_view_data(p_submesh.indices.binary, "index");
}

bool vertex_layouts_equal(const RUVertexView& p_left, const RUVertexView& p_right) {
    if (p_left.vertex_stride != p_right.vertex_stride || p_left.attribute_count != p_right.attribute_count) {
        return false;
    }
    for (size_t i = 0; i < p_left.attribute_count; ++i) {
        const RUVertexView::Attribute& left = p_left.attributes.get()[i];
        const RUVertexView::Attribute& right = p_right.attributes.get()[i];
        if (left.role.hash != right.role.hash || left.format != right.format || left.offset != right.offset ||
            left.size != right.size || left.role.get_name() != right.role.get_name()) {
            return false;
        }
    }
    return true;
}

bool vertex_layout_less(const RUVertexView& p_left, const RUVertexView& p_right) {
    if (p_left.vertex_stride != p_right.vertex_stride) {
        return p_left.vertex_stride < p_right.vertex_stride;
    }
    if (p_left.attribute_count != p_right.attribute_count) {
        return p_left.attribute_count < p_right.attribute_count;
    }
    for (size_t i = 0; i < p_left.attribute_count; ++i) {
        const RUVertexView::Attribute& left = p_left.attributes.get()[i];
        const RUVertexView::Attribute& right = p_right.attributes.get()[i];
        const std::string_view left_role = left.role.get_name();
        const std::string_view right_role = right.role.get_name();
        if (left_role != right_role) {
            return left_role < right_role;
        }
        if (left.format != right.format) {
            return left.format < right.format;
        }
        if (left.offset != right.offset) {
            return left.offset < right.offset;
        }
        if (left.size != right.size) {
            return left.size < right.size;
        }
    }
    return false;
}

void register_draw_group_submesh(
    Vector<DrawGroupMeshInfo, RenderAllocator>& p_draw_group_infos, RUSubmesh3D& p_submesh, RenderAllocator* p_allocator) {
    auto& material = p_submesh.material;
    if (material == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: submesh material should not be NULL.");
    }
    if (material->resource_key == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: submesh material resource key should not be NULL.");
    }
    if (material->graphics_pipeline == nullptr || material->graphics_pipeline->resource_key == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: submesh material graphics pipeline should not be NULL.");
    }
    validate_vertex_data(p_submesh.vertices);
    validate_index_data(p_submesh);
    std::string_view material_key = material->resource_key;
    std::string_view graphics_pipeline_key = material->graphics_pipeline->resource_key;
    auto draw_group = std::find_if(p_draw_group_infos.begin(), p_draw_group_infos.end(), [&](const DrawGroupMeshInfo& p_info) {
        return p_info.graphics_pipeline_key == graphics_pipeline_key && p_info.material_key == material_key &&
               vertex_layouts_equal(*p_info.vertex_data, p_submesh.vertices);
    });
    if (draw_group == p_draw_group_infos.end()) {
        p_draw_group_infos.push_back(DrawGroupMeshInfo{.graphics_pipeline_key = graphics_pipeline_key,
            .material_key = material_key,
            .material = material,
            .vertex_data = &p_submesh.vertices,
            .submeshes = Vector<RUSubmesh3D*, RenderAllocator>(p_allocator)});
        draw_group = p_draw_group_infos.end() - 1;
    }
    draw_group->submeshes.push_back(&p_submesh);
}

void copy_vertex_layout(RenderDatabase::OpaqueTrianglesInfo::VertexLayout& p_target, const RUVertexView& p_vertex_data) {
    p_target.vertex_stride = p_vertex_data.vertex_stride;
    p_target.attributes.reserve(p_vertex_data.attribute_count);
    for (size_t i = 0; i < p_vertex_data.attribute_count; ++i) {
        const RUVertexView::Attribute& source = p_vertex_data.attributes.get()[i];
        p_target.attributes.push_back(RenderDatabase::OpaqueTrianglesInfo::VertexAttribute{
            .role = ShaderRole(source.role.get_name()), .format = source.format, .offset = source.offset, .size = source.size});
    }
}

void append_submesh_geometry(
    RenderDatabase::OpaqueTrianglesInfo& p_triangles_info, const RUSubmesh3D& p_submesh, uint32_t p_first_vertex, uint32_t p_obj_id) {
    const RUVertexView& vertex_data = p_submesh.vertices;
    if (vertex_data.vertex_count > std::numeric_limits<uint32_t>::max() - p_first_vertex) {
        throw std::runtime_error("Failed to set opaque triangles: merged draw-group vertex count exceeds uint32.");
    }
    const size_t vertex_start = p_triangles_info.vertices.size();
    if (vertex_data.binary.size > std::numeric_limits<size_t>::max() - vertex_start) {
        throw std::runtime_error("Failed to set opaque triangles: merged vertex byte size overflows.");
    }
    p_triangles_info.vertices.resize(vertex_start + vertex_data.binary.size);
    if (vertex_data.binary.size > 0) {
        std::memcpy(p_triangles_info.vertices.data() + vertex_start,
            get_binary_view_data(vertex_data.binary, "vertex"),
            vertex_data.binary.size);
    }
    const auto* indices = reinterpret_cast<const uint32_t*>(get_binary_view_data(p_submesh.indices.binary, "index"));
    for (size_t i = 0; i < p_submesh.indices.index_count; ++i) {
        const uint32_t index = indices[i];
        if (index >= vertex_data.vertex_count) {
            throw std::runtime_error(std::format(
                "Failed to set opaque triangles: submesh index {} is out of bounds for {} vertices.", index, vertex_data.vertex_count));
        }
        p_triangles_info.indices.push_back(p_first_vertex + index);
    }
    if (p_submesh.indices.index_count > 0) {
        p_triangles_info.obj_id.push_back(p_obj_id);
    }
}

size_t get_or_append_material(RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    std::unordered_map<std::string_view, size_t>& p_registered_material_indices,
    const DrawGroupMeshInfo& p_draw_group_info) {
    auto registered = p_registered_material_indices.find(p_draw_group_info.material_key);
    if (registered != p_registered_material_indices.end()) {
        return registered->second;
    }
    const size_t material_index = p_triangles_info.materials.size();
    p_triangles_info.materials.push_back(RenderDatabase::OpaqueTrianglesInfo::MaterialUnit{.material = p_draw_group_info.material});
    p_registered_material_indices.insert({p_draw_group_info.material_key, material_index});
    return material_index;
}

bool draw_group_has_indices(const DrawGroupMeshInfo& p_draw_group_info) {
    return std::ranges::any_of(p_draw_group_info.submeshes, [](const RUSubmesh3D* p_submesh) {
        return p_submesh->indices.index_count > 0;
    });
}

bool append_draw_group(RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const DrawGroupMeshInfo& p_draw_group_info,
    size_t p_material_index,
    RenderAllocator* p_allocator,
    const uint32_t* p_preserved_object_ids,
    size_t p_preserved_object_id_count,
    size_t& p_preserved_object_id_index) {
    if (!draw_group_has_indices(p_draw_group_info)) {
        return false;
    }
    static std::atomic<uint32_t> obj_id_tracker = 0;
    const uint32_t new_object_id = p_preserved_object_id_count == 0 ? obj_id_tracker.fetch_add(1, std::memory_order_acq_rel) : 0;
    RenderDatabase::OpaqueTrianglesInfo::DrawGroup draw_group(p_allocator);
    draw_group.vertex_buffer_byte_offset = p_triangles_info.vertices.size();
    draw_group.index_sub_region.starting_index = p_triangles_info.indices.size();
    draw_group.material_index = p_material_index;
    draw_group.indirect_command_sub_region = {.starting_index = p_triangles_info.draw_groups.size(), .size = 1};
    copy_vertex_layout(draw_group.vertex_layout, *p_draw_group_info.vertex_data);

    uint32_t first_vertex = 0;
    for (const auto* submesh : p_draw_group_info.submeshes) {
        if (submesh->indices.index_count == 0) {
            continue;
        }
        uint32_t object_id = new_object_id;
        if (p_preserved_object_id_count > 0) {
            if (p_preserved_object_id_index >= p_preserved_object_id_count) {
                throw std::runtime_error("Failed to refresh opaque triangles: preserved object IDs do not match the mesh.");
            }
            object_id = p_preserved_object_ids[p_preserved_object_id_index++];
        }
        append_submesh_geometry(p_triangles_info, *submesh, first_vertex, object_id);
        first_vertex += static_cast<uint32_t>(submesh->vertices.vertex_count);
    }
    draw_group.index_sub_region.size = p_triangles_info.indices.size() - draw_group.index_sub_region.starting_index;
    p_triangles_info.draw_groups.push_back(std::move(draw_group));
    return true;
}

} // namespace

RenderDatabase::RenderDatabase(AllocType* p_allocator) : compiled_render_resources(p_allocator), dynamic_opaque(p_allocator) {
    singleton = this;
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create renderer database: allocator is null.");
    }
    allocator = p_allocator;
}

void RenderDatabase::register_compiled_render_resource(std::string_view p_resource_key, Ref<IRenderObject> p_render_object) {
    if (p_resource_key.empty()) {
        throw std::runtime_error("Failed to register compiled render resource: resource key is empty.");
    }
    if (p_render_object == MEM_NULL) {
        throw std::runtime_error("Failed to register compiled render resource: render object is null.");
    }
    compiled_render_resources.insert_or_assign(CompiledRenderResourceKey{.resource_key = std::string(p_resource_key),
                                                   .render_object_type_id = p_render_object->get_type_uuid()},
        RefWeak<IRenderObject>(p_render_object));
}

Ref<IRenderObject> RenderDatabase::get_compiled_render_resource(std::string_view p_resource_key, HashCode p_render_object_type_id) {
    auto entry = compiled_render_resources.find(
        CompiledRenderResourceKey{.resource_key = std::string(p_resource_key), .render_object_type_id = p_render_object_type_id});
    if (entry == compiled_render_resources.end() || entry->second.is_null()) {
        return MEM_NULL;
    }
    return entry->second.lock();
}

bool RenderDatabase::contains_compiled_render_resource(std::string_view p_resource_key, HashCode p_render_object_type_id) const {
    auto entry = compiled_render_resources.find(
        CompiledRenderResourceKey{.resource_key = std::string(p_resource_key), .render_object_type_id = p_render_object_type_id});
    return entry != compiled_render_resources.end() && entry->second.is_valid();
}

void RenderDatabase::clear_compiled_render_resources() {
    compiled_render_resources.clear();
}

void RenderDatabase::set_opaque_triangles_linear(
    OpaqueTrianglesInfo& p_triangles_info, RUMesh3D* p_mesh, const uint32_t* p_preserved_object_ids, size_t p_preserved_object_id_count) {
    if (p_mesh == nullptr) {
        throw std::runtime_error("Failed to set opaque triangles: mesh "
                                 "resource should not be NULL.");
    }
    p_triangles_info.reset();

    Vector<DrawGroupMeshInfo> draw_group_infos(allocator);

    size_t total_triangles = 0;
    std::span<RefRes<RUSubmesh3D>> submeshes(p_mesh->submeshes.get(), p_mesh->submesh_count);
    for (auto& submesh : submeshes) {
        if (submesh == nullptr) {
            throw std::runtime_error("Failed to set opaque triangles: submesh should not be NULL.");
        }
        register_draw_group_submesh(draw_group_infos, *submesh, allocator);
        total_triangles += submesh->indices.index_count / 3;
    }
    p_triangles_info.obj_id.reserve(submeshes.size());
    p_triangles_info.indices.reserve(total_triangles * 3);

    std::sort(draw_group_infos.begin(), draw_group_infos.end(), [](const DrawGroupMeshInfo& p_left, const DrawGroupMeshInfo& p_right) {
        if (p_left.graphics_pipeline_key != p_right.graphics_pipeline_key) {
            return p_left.graphics_pipeline_key < p_right.graphics_pipeline_key;
        }
        if (vertex_layout_less(*p_left.vertex_data, *p_right.vertex_data)) {
            return true;
        }
        if (vertex_layout_less(*p_right.vertex_data, *p_left.vertex_data)) {
            return false;
        }
        return p_left.material_key < p_right.material_key;
    });

    std::unordered_map<std::string_view, size_t> registered_material_indices;
    size_t preserved_object_id_index = 0;
    for (const auto& draw_group_info : draw_group_infos) {
        if (!draw_group_has_indices(draw_group_info)) {
            continue;
        }
        const size_t material_index = get_or_append_material(p_triangles_info, registered_material_indices, draw_group_info);
        const size_t draw_group_index = p_triangles_info.draw_groups.size();
        if (!append_draw_group(
                p_triangles_info, draw_group_info, material_index, allocator, p_preserved_object_ids, p_preserved_object_id_count, preserved_object_id_index)) {
            continue;
        }
        if (p_triangles_info.graphics_pipelines.empty() ||
            p_triangles_info.graphics_pipelines.back().graphics_pipeline != draw_group_info.material->graphics_pipeline) {
            p_triangles_info.graphics_pipelines.push_back(OpaqueTrianglesInfo::GraphicsPipelineUnit{
                .graphics_pipeline = draw_group_info.material->graphics_pipeline,
                .draw_group_sub_region = {.starting_index = draw_group_index, .size = 0},
            });
        }
        ++p_triangles_info.graphics_pipelines.back().draw_group_sub_region.size;
    }
    if (p_preserved_object_id_count > 0 && preserved_object_id_index != p_preserved_object_id_count) {
        throw std::runtime_error("Failed to refresh opaque triangles: preserved object IDs do not match the mesh.");
    }
}
} // namespace WhiteBirdEngine
