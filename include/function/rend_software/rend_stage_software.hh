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
#ifndef WBE_FILE_REND_STAGE_SOFTWARE
#define WBE_FILE_REND_STAGE_SOFTWARE

#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_software/rend_software_defs.hh"
#include "function/rend_software/ro_framebuffer_software.hh"
#include "function/rend_software/ro_image_software.hh"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_uint2.hpp"
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <span>
namespace WhiteBirdEngine {

struct VertexDataSoftware {
    VertexDataSoftware(RendAlloc* p_alloc) : layout(p_alloc) {
    }
    struct ElementDescription {
        enum class Type {
            FLOAT32,
            INT8,
        };
        Type type{};
        uint32_t element_offset = 0;
        uint32_t element_stride = 0;
        uint32_t element_count = 0;
    };

    std::span<uint8_t> buffer;
    Vector<ElementDescription, RendAlloc> layout;
    uint32_t vertex_stride = 0;
    uint32_t element_count = 0;
};

struct FragmentSoftware {
    glm::uvec2 coordinate;
    float depth;
    glm::vec3 barycentric;
};

struct RendPerThreadScreenSpaceJobSoftware {};

struct RendContextSoftware {
    RendContextSoftware(BaseRendDataSoftware* p_rend_data)
        : base_rend_data(p_rend_data), input_buffer(p_rend_data->main_alloc),
          vertex_in(p_rend_data->main_alloc), vertex_out(p_rend_data->main_alloc) {
    }
    BaseRendDataSoftware* base_rend_data;

    Vector<uint8_t, RendAlloc> input_buffer;

    VertexDataSoftware vertex_in;
    std::span<uint32_t> indeices;
    VertexDataSoftware vertex_out;
    std::span<FragmentSoftware> fragments;

    Ref<ROImageSoftware> depth;
    Ref<ROImageSoftware> color;
};

class RendStagesSoftware final {
public:
    using RendStageType = void (*)(RendContextSoftware*);

    struct Builder {
        Builder& set_vertex_shading_stage(RendStageType p_stage) {
            vertex_shading_stage = p_stage;
            return *this;
        }
        Builder& set_rasterization_stage(RendStageType p_stage) {
            rasterization_stage = p_stage;
            return *this;
        }
        Builder& set_fragment_shading_stage(RendStageType p_stage) {
            fragment_shading_stage = p_stage;
            return *this;
        }
        Ref<RendStagesSoftware> build_ref(BaseRendDataSoftware* p_rend_data) const {
            return make_ref<RendStagesSoftware>(
                p_rend_data->main_alloc, vertex_shading_stage, rasterization_stage, fragment_shading_stage);
        }
        RendStageType vertex_shading_stage = default_vertex_shading_stage;
        RendStageType rasterization_stage = default_rasterization_stage;
        RendStageType fragment_shading_stage = default_fragment_shading_stage;
    };

    RendStagesSoftware();
    ~RendStagesSoftware() = default;
    RendStagesSoftware(const RendStagesSoftware&) = delete;
    RendStagesSoftware(RendStagesSoftware&&) = delete;
    RendStagesSoftware& operator=(const RendStagesSoftware&) = delete;
    RendStagesSoftware& operator=(RendStagesSoftware&&) = delete;

    RendStagesSoftware(RendStageType p_vertex_shading_stage, RendStageType p_rasterization_stage, RendStageType p_fragment_shading_stage);

    Vector<RendStageType, RendAlloc> get_graphics_pipeline(BaseRendDataSoftware* p_rend_context) const {
        return Vector<RendStageType, RendAlloc>(
            {vertex_shading_stage, rasterization_stage, fragment_shading_stage}, p_rend_context->main_alloc);
    }

private:
    static void default_vertex_shading_stage(RendContextSoftware* p_rend_context) noexcept;
    static void default_rasterization_stage(RendContextSoftware* p_rend_context) noexcept;
    static void default_fragment_shading_stage(RendContextSoftware* p_rend_context) noexcept;

    RendStageType vertex_shading_stage;
    RendStageType rasterization_stage;
    RendStageType fragment_shading_stage;
};

} // namespace WhiteBirdEngine

#endif
