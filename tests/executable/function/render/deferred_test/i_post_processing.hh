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
#ifndef WBE_FILE_I_POST_PROCESSING_HH
#define WBE_FILE_I_POST_PROCESSING_HH

#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class IPostProcessing
 * @brief Interface of a post-processing effect of the deferred rendering test that modifies a color image in place.
 * @details Effects are disabled on construction. A disabled effect keeps its tasks in every graph it was added to, but
 * they record no commands, so the color image passes through unchanged. The enabled state is read when each task is
 * recorded, so it must only change between frames. Neither copyable nor movable. All methods and destruction are confined
 * to the rendering thread, and the effect must outlive every graph it was added to.
 */
class IPostProcessing {
public:
    IPostProcessing() = delete;
    /**
     * @brief Constructor.
     * @param p_allocator Allocator of the effect's tasks; must outlive the effect.
     */
    explicit IPostProcessing(RenderAllocator* p_allocator);
    virtual ~IPostProcessing() = default;
    IPostProcessing(const IPostProcessing&) = delete;
    IPostProcessing(IPostProcessing&&) = delete;
    IPostProcessing& operator=(const IPostProcessing&) = delete;
    IPostProcessing& operator=(IPostProcessing&&) = delete;

    /**
     * @brief Human-readable name of the effect.
     */
    virtual std::string_view get_name() const = 0;

    /**
     * @brief Adds the effect's resources and tasks to a graph, ordered after a previous user of the color image.
     * @param p_graph Graph descriptor that already contains p_previous_task_id.
     * @param p_color_image_view Graph id of the color image the effect modifies in place; it must be sampled and storage.
     * @param p_previous_task_id Id of the last task that accesses p_color_image_view before the effect.
     * @return Id of the effect's last task that accesses p_color_image_view; later users of the image must be connected
     * to it.
     */
    virtual std::string_view add_render_tasks(
        RendGraphDescVK& p_graph, std::string_view p_color_image_view, std::string_view p_previous_task_id) const = 0;

    /**
     * @brief Whether the effect's tasks record their commands.
     */
    bool is_enabled() const {
        return enabled;
    }

    /**
     * @brief Enable or disable the effect from the next recorded frame.
     * @param p_enabled Whether the effect's tasks record their commands.
     */
    void set_enabled(bool p_enabled) {
        enabled = p_enabled;
    }

protected:
    /**
     * @brief Wrap a task so that it records its commands only while the effect is enabled.
     * @details Binding, queue selection, and access reports always reach p_task.
     * @param p_task The task to wrap; must not be null.
     * @return The wrapping task.
     * @throws std::runtime_error If p_task is null.
     */
    Unique<IRenderTaskVK> make_toggleable_task(Unique<IRenderTaskVK> p_task) const;

    /**
     * @brief Get the allocator of the effect's tasks.
     */
    RenderAllocator* get_allocator() const {
        return allocator;
    }

private:
    RenderAllocator* allocator = nullptr;
    bool enabled = false;
};

} // namespace WhiteBirdEngine

#endif
