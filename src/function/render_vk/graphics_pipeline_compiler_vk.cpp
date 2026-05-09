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
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_resource_unit.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <format>
#include <glm/ext/vector_float4.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

class RTRunCompiledGraphicsPipelineVK final : public RenderTask {
public:
    RTRunCompiledGraphicsPipelineVK(
        const BaseRenderDataVK* p_render_data, RendererVK* p_renderer, RendPipelineDescVK&& p_pipeline_desc)
        : renderer(p_renderer), graph(make_ref<RenderGraphVK>(p_render_data->allocator, p_render_data)) {
        if (renderer == nullptr) {
            throw std::runtime_error("Failed to create compiled graphics pipeline task: renderer is null.");
        }
        graph->build_from_descriptor(p_pipeline_desc);
    }

    virtual void init() override {
    }

    virtual void perform(double p_delta) override {
        renderer->execute_graph(graph, p_delta);
    }

private:
    RendererVK* renderer = nullptr;
    Ref<RenderGraphVK> graph;
};

class GraphicsPipelineCompilerVKImpl {
public:
    GraphicsPipelineCompilerVKImpl(const GraphicsPipelineCompilerVKContext& p_context) : context(p_context) {
        if (context.allocator == nullptr || context.render_data == nullptr || context.render_database == MEM_NULL || context.render_pass == MEM_NULL ||
            context.renderer == nullptr) {
            throw std::runtime_error("Failed to create graphics pipeline compiler: invalid context.");
        }
    }

    Unique<RenderTask> compile(const std::string& p_resource_key, RefRes<RUGraphicsPipeline> p_pipeline_resource) {
        if (p_pipeline_resource->vertex_stage == nullptr || p_pipeline_resource->fragment_stage == nullptr) {
            throw std::runtime_error(std::format("Failed to compile graphics pipeline '{}': vertex and fragment stages are required.", p_resource_key));
        }
        auto vertex_shader = compile_shader(p_pipeline_resource->vertex_stage);
        auto fragment_shader = compile_shader(p_pipeline_resource->fragment_stage);
        auto descriptor_set_layouts = compile_descriptor_set_layouts(p_resource_key, *p_pipeline_resource);
        auto pipeline_layout = compile_pipeline_layout(p_resource_key, descriptor_set_layouts);
        auto graphics_pipeline = compile_graphics_pipeline(p_resource_key, *p_pipeline_resource, vertex_shader, fragment_shader, pipeline_layout);
        auto pipeline_desc = build_pipeline_desc(p_resource_key, graphics_pipeline);
        return make_unique<RTRunCompiledGraphicsPipelineVK>(context.allocator, context.render_data, context.renderer, std::move(pipeline_desc));
    }

private:
    GraphicsPipelineCompilerVKContext context;

    // Shader resources are cached by their own manifest keys because multiple
    // graphics pipelines can point at the same shader resource unit.
    Ref<ROShaderVK> compile_shader(RefRes<RUShader> p_shader) {
        if (p_shader == nullptr || p_shader->resource_key == nullptr) {
            throw std::runtime_error("Failed to compile shader: shader resource key was not propagated.");
        }
        std::string shader_key = p_shader->resource_key;
        auto registered_shader = get_registered_render_object<ROShaderVK>(shader_key, TypeUUID<ROShaderVK>::UUID);
        if (registered_shader != MEM_NULL) {
            return registered_shader;
        }
        auto shader = ROShaderVK::Builder().set_code(p_shader).create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(shader_key, TypeUUID<ROShaderVK>::UUID, RefWeak<IRenderObject>(shader));
        return shader;
    }

    std::vector<Ref<RODescriptorSetLayoutVK>> compile_descriptor_set_layouts(
        const std::string& p_resource_key, const RUGraphicsPipeline& p_pipeline_resource) {
        std::vector<Ref<RODescriptorSetLayoutVK>> result;
        auto descriptor_set_layout_bindings = build_graphics_pipeline_descriptor_set_layout_bindings(p_pipeline_resource);
        result.reserve(descriptor_set_layout_bindings.size());
        for (const auto& descriptor_set_layout_binding : descriptor_set_layout_bindings) {
            // Descriptor set layouts are compiler-generated subresources of the
            // graphics pipeline, so they use derived resource keys.
            std::string descriptor_set_layout_key = std::format("{}#descriptor_set_layout_{}", p_resource_key, descriptor_set_layout_binding.set_index);
            auto descriptor_set_layout = get_registered_render_object<RODescriptorSetLayoutVK>(
                descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID);
            if (descriptor_set_layout == MEM_NULL) {
                RODescriptorSetLayoutVK::Builder builder;
                for (const auto& binding : descriptor_set_layout_binding.bindings) {
                    builder.add_binding(binding);
                }
                descriptor_set_layout = builder.create_ref(context.render_data);
                context.render_database->register_compiled_render_resource(
                    descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID, RefWeak<IRenderObject>(descriptor_set_layout));
            }
            result.push_back(descriptor_set_layout);
        }
        return result;
    }

    Ref<ROPipelineLayoutVK> compile_pipeline_layout(
        const std::string& p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts) {
        std::string pipeline_layout_key = std::format("{}#pipeline_layout", p_resource_key);
        auto pipeline_layout = get_registered_render_object<ROPipelineLayoutVK>(pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID);
        if (pipeline_layout != MEM_NULL) {
            return pipeline_layout;
        }
        ROPipelineLayoutVK::Builder builder;
        for (const auto& descriptor_set_layout : p_descriptor_set_layouts) {
            builder.add_descriptor_set_layout(descriptor_set_layout);
        }
        pipeline_layout = builder.create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID, RefWeak<IRenderObject>(pipeline_layout));
        return pipeline_layout;
    }

    Ref<ROGraphicsPipelineVK> compile_graphics_pipeline(const std::string& p_resource_key,
        const RUGraphicsPipeline& p_pipeline_resource,
        Ref<ROShaderVK> p_vertex_shader,
        Ref<ROShaderVK> p_fragment_shader,
        Ref<ROPipelineLayoutVK> p_pipeline_layout) {
        auto graphics_pipeline = get_registered_render_object<ROGraphicsPipelineVK>(p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID);
        if (graphics_pipeline != MEM_NULL) {
            return graphics_pipeline;
        }
        ROGraphicsPipelineVK::Config::ShaderConfig shader_config;
        shader_config.set_shaders(
            p_vertex_shader, p_fragment_shader, get_stage_entry(p_pipeline_resource.vertex_stage_entry), get_stage_entry(p_pipeline_resource.fragment_stage_entry));
        auto pipeline_builder = ROGraphicsPipelineVK::Builder().default_render_opaque_configuration(
            context.render_data, shader_config, p_pipeline_layout, context.render_pass);
        pipeline_builder.config.depth_test.set_depth_test(p_pipeline_resource.depth_test ? VK_TRUE : VK_FALSE);
        pipeline_builder.config.depth_test.set_depth_write(p_pipeline_resource.depth_write ? VK_TRUE : VK_FALSE);
        graphics_pipeline = pipeline_builder.create_ref(context.render_data);
        context.render_database->register_compiled_render_resource(
            p_resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID, RefWeak<IRenderObject>(graphics_pipeline));
        return graphics_pipeline;
    }

    RendPipelineDescVK build_pipeline_desc(const std::string& p_resource_key, Ref<ROGraphicsPipelineVK> p_graphics_pipeline) const {
        RendResID pipeline_res_id = dynam_hash(p_resource_key);
        if (pipeline_res_id == REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK || pipeline_res_id == REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK ||
            pipeline_res_id == REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK || pipeline_res_id == REND_RES_STATIC_OPAQUE_TRIANGLES_OBJ_IDS_VK) {
            throw std::runtime_error(std::format("Failed to compile graphics pipeline '{}': generated render resource id collides.", p_resource_key));
        }
        std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
        RendPipelineDescVK pipeline_desc(context.allocator);
        pipeline_desc.resources.emplace_back(RendResVK{
            .res_id = pipeline_res_id,
            .res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID,
            .res = p_graphics_pipeline,
        });
        RendTaskDescVK task_desc(context.allocator);
        task_desc.task_id = dynam_hash(std::format("{}#run_graphics_pipeline", p_resource_key));
        // Material descriptor sets are still supplied by future task wiring;
        // the generated task currently owns the compiled pipeline object.
        task_desc.create = [render_allocator = context.allocator,
                               render_data = context.render_data,
                               pipeline_res_id,
                               descriptor_sets]() -> Unique<RenderTaskVK> {
            return make_unique<RTRunGraphicsPipelineVK>(render_allocator,
                render_data,
                glm::vec4(0.0F, 0.0F, 0.0F, 1.0F),
                REND_RES_SWAP_CHAIN_FRAMEBUFFER_VK,
                pipeline_res_id,
                REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
                REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
                std::span<const Ref<RODescriptorSetVK>>(descriptor_sets.data(), descriptor_sets.size()));
        };
        pipeline_desc.tasks.emplace_back(std::move(task_desc));
        return pipeline_desc;
    }

    template <typename RenderObjectType>
    Ref<RenderObjectType> get_registered_render_object(const std::string& p_resource_key, HashCode p_render_object_type_id) {
        auto render_object = context.render_database->get_compiled_render_resource(p_resource_key, p_render_object_type_id);
        if (render_object == MEM_NULL) {
            return MEM_NULL;
        }
        return render_object.dynamic_cast_ref<RenderObjectType>();
    }

    static std::string get_stage_entry(RefRes<char> p_stage_entry) {
        return p_stage_entry == nullptr ? std::string("main") : std::string(p_stage_entry.get());
    }
};

} // namespace

Unique<RenderTask> compile_graphics_pipeline_task_vk(
    const GraphicsPipelineCompilerVKContext& p_context, const std::string& p_resource_key, RefRes<RUGraphicsPipeline> p_pipeline_resource) {
    GraphicsPipelineCompilerVKImpl compiler(p_context);
    return compiler.compile(p_resource_key, p_pipeline_resource);
}

} // namespace WhiteBirdEngine
