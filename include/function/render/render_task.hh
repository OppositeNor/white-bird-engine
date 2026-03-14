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
#ifndef WBE_FILE_RENDER_TASK_HH
#define WBE_FILE_RENDER_TASK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_database.hh"
#include "function/render/i_render_object.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <set>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

using RendResID = HashCode;
using RendResTID = HashCode;
using RendTaskID = HashCode;

WBE_LABEL(RT_UNDEFINED, WBE_RENDER_TASK);

/**
 * @class RendResInfo
 * @brief Render resource info.
 *
 */
struct RendResInfo {
    // The ID of this render resource. Each resource should have a unique res_id.
    RendResID res_id;
    // The type of the render reesource. Each type should have a unique type_id.
    RendResTID type_id;
};

/**
 * @class RenderTask
 * @brief The abstract struct for render task.
 *
 */
struct RenderTask {
    WBE_R6_DEFAULT_VIRTUAL(RenderTask)

    /**
     * @brief Initialize the render task (since render task does not follow RAII, meaning
     * calling the constructor does not fully initialize a render task, this method could provide
     * a place where the full initialization could be performed).
     */
    virtual void init() = 0;

    /**
     * @brief Perform the render task.
     */
    virtual void perform(double p_delta) = 0;
};

/**
 * @class RendPipelineInfo
 * @brief The information of a render pipeline.
 *
 */
struct RendPipelineInfo {
    /**
     * @class Resource
     * @brief Represents a resource.
     *
     */
    struct Resource {
        // The ID of the resource.
        RendResID res_id;
        // The type ID of the resource.
        RendResTID type_id;
        // Is the resource a weak binding.
        bool is_weak = false;
        // The resource instance. If is_weak is false, the previously registered resource with
        // the same res_id will be replaced by this instance. If is_weak is true, it
        // will try to find resources that was registered before by other pipelines, and
        // use those as the resource here. If none reference was found, it will be replaced
        // with this one. When is_weak is true, or when is_weak is false and there is no previously
        // registered resource with the same res_id, res should not be MEM_NULL.
        Ref<IRenderObject> res = MEM_NULL;
    };

    /**
     * @class PFIFResource
     * @brief Per frame in flight resource.
     *
     */
    struct PFIFResource {
        // The ID of the resource.
        RendResID res_id;
        // The type ID of the resource.
        RendResTID type_id;
        // Is the resource a weak binding.
        bool is_weak = false;
        // The resource instance. If is_weak is false, the previously registered resource with
        // the same res_id will be replaced by this instance. If is_weak is true, it
        // will try to find resources that was registered before by other pipelines, and
        // use those as the resource here. If none reference was found, it will be replaced
        // with this one. When is_weak is true, or when is_weak is false and there is no previously
        // registered resource with the same res_id, res should not be empty.
        // A PFIFResource is considered NULL when its res field is empty.
        Vector<Ref<IRenderObject>> res;
    };

    // The root task of the pipeline.
    Ref<struct RendTaskInfo> root_task;
    // All the resources used in the render pipeline.
    std::vector<Resource> resources;

    /**
     * @brief Set the root task of the pipeline.
     *
     * @param p_root_task The root task.
     * @return *this.
     */
    RendPipelineInfo& set_root_task(Ref<RendTaskInfo> p_root_task) {
        root_task = p_root_task;
        return *this;
    }

    /**
     * @brief Add a resource used in the render pipeline.
     *
     * @param p_resource The resource to add.
     * @return *this.
     */
    RendPipelineInfo& add_resource(Resource p_resource) {
        resources.push_back(p_resource);
        return *this;
    }

    /**
     * @brief Add a resource used in the render pipeline.
     *
     * @param p_res_id The resource ID.
     * @param p_type_id The resource type ID.
     * @param p_res The resource instance.
     * @param p_is_weak Is the resource a weak binding.
     * @return *this.
     */
    RendPipelineInfo& add_resource(RendResID p_res_id, RendResTID p_type_id, Ref<IRenderObject> p_res, bool p_is_weak = false) {
        resources.push_back({p_res_id, p_type_id, p_is_weak, p_res});
        return *this;
    }
};

/**
 * @class RendTaskInfo
 * @brief Render task info.
 *
 */
struct RendTaskInfo {

    /**
     * @class Builder
     * @brief Builder for RendTaskInfo
     *
     */
    struct Builder {
        // The ID of this render task. Each render task should have a unique task_id.
        Required<RendTaskID> task_id;
        // Previous render task.
        std::set<Ref<RendTaskInfo>> prev;
        // Next render tasks.
        std::set<Ref<RendTaskInfo>> next;
        // Required resources.
        std::vector<RendResInfo> req_res;
        // The render task instance.
        Ref<class IRenderTaskFactory> render_task_factory = MEM_NULL;

        /**
         * @brief Set the task ID.
         *
         * @param p_task_id The task ID to set to.
         * @return *this.
         */
        Builder& set_task_id(RendTaskID p_task_id) {
            task_id = p_task_id;
            return *this;
        }
        
        /**
         * @brief Add a previous task.
         *
         * @param p_prev_task The previous task.
         * @return *this.
         */
        Builder& add_prev(Ref<RendTaskInfo> p_prev_task) {
            prev.insert(p_prev_task);
            return *this;
        }

        /**
         * @brief Add a next task.
         *
         * @param p_next_task The next task.
         * @return *this.
         */
        Builder& add_next(Ref<RendTaskInfo> p_next_task) {
            next.insert(p_next_task);
            return *this;
        }

        /**
         * @brief Add a required resource of the task.
         *
         * @param p_res_info The resource information.
         * @return *this.
         */
        Builder& add_req_res(RendResInfo p_res_info) {
            req_res.push_back(p_res_info);
            return *this;
        }

        /**
         * @brief Add a required resource of the task.
         *
         * @param p_res_id The resource ID.
         * @param p_type_id The resource type ID.
         * @return *this.
         */
        Builder& add_req_res(RendResID p_res_id, RendResTID p_type_id) {
            req_res.push_back({p_res_id, p_type_id});
            return *this;
        }

        /**
         * @brief Set the render task factory.
         *
         * @param p_factory The render task factory.
         * @return *this.
         */
        Builder& set_render_task_factory(Ref<IRenderTaskFactory> p_factory) {
            render_task_factory = p_factory;
            return *this;
        }

        Ref<RendTaskInfo> build(RenderAllocator* p_allocator) const {
            auto result = make_ref<RendTaskInfo>(p_allocator);
            result->task_id = WBE_REQUIRED(task_id);

            // Resolve previous tasks
            result->prev.reserve(prev.size());
            for (auto prev_task : prev) {
                result->prev.push_back(prev_task->task_id);
                prev_task->next.emplace_back(result);
            }

            // Resolve next tasks
            result->next.reserve(next.size());
            for (auto next_task : next) {
                result->next.push_back(next_task);
                next_task->prev.push_back(result->task_id);
            }

            result->req_res = req_res; // Considering this builder might be reused, we are not moving the data but copying instead.
            result->render_task_factory = WBE_REQUIRES_VALID(render_task_factory);
            return result;
        }
    };

    // The ID of this render task. Each render task should have a unique task_id.
    RendTaskID task_id = RT_UNDEFINED;
    // Previous render task.
    std::vector<RendTaskID> prev;
    // Next render tasks.
    std::vector<Ref<RendTaskInfo>> next;
    // Required resources.
    std::vector<RendResInfo> req_res;
    // The render task instance.
    Ref<IRenderTaskFactory> render_task_factory = MEM_NULL;
};

/**
 * @class IRenderTaskFactory
 * @brief The factory to create RenderTask.
 *
 */
class IRenderTaskFactory {
public:
    WBE_R6_DEFAULT_VIRTUAL(IRenderTaskFactory)

    /**
     * @brief Create the render task instance.
     */
    virtual Ref<RenderTask> create() const = 0;
};

} // namespace WhiteBirdEngine

#endif
