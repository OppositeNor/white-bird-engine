/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_I_VOXEL_RENDER_FEATURE_HH
#define WBE_FILE_I_VOXEL_RENDER_FEATURE_HH

#include "function/render_vk/render_graph_descriptor_vk.hh"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace WhiteBirdEngine {

class DynamicMeshRenderFeature;

/**
 * @enum VoxelDebugViewMode
 * @brief Selects the voxel construction state visualized by the deferred rendering test.
 * @details Each voxel structure names what it draws for a mode through IVoxelRenderFeature::get_debug_view_name().
 */
enum class VoxelDebugViewMode : uint32_t {
    DISABLED,
    POST_SVO_BUILD,
    PRE_DEFERRED_PROPAGATION,
};

/**
 * @brief Triangle-list indices of the unit cube whose corner i has the coordinates ((i >> 2) & 1, (i >> 1) & 1, i & 1).
 * @details The voxel debug draws instance this cube per leaf or node and generate its corners in the vertex shader.
 */
inline constexpr std::array<uint32_t, 36> VOXEL_DEBUG_CUBE_INDICES{
    0,
    2,
    3,
    0,
    3,
    1,
    4,
    5,
    7,
    4,
    7,
    6,
    0,
    1,
    5,
    0,
    5,
    4,
    2,
    6,
    7,
    2,
    7,
    3,
    0,
    4,
    6,
    0,
    6,
    2,
    1,
    3,
    7,
    1,
    7,
    5,
};

/**
 * @class IVoxelRenderFeature
 * @brief Voxel structure built over the deferred test's scene, with its construction and debug-overlay render tasks.
 * @details Implementations own their buffers and pipelines. Neither copyable nor movable. All methods and destruction are
 * confined to the rendering thread; registered graphs retain the registered resources and task factories.
 */
class IVoxelRenderFeature {
public:
    IVoxelRenderFeature() = default;
    virtual ~IVoxelRenderFeature() = default;
    IVoxelRenderFeature(const IVoxelRenderFeature&) = delete;
    IVoxelRenderFeature(IVoxelRenderFeature&&) = delete;
    IVoxelRenderFeature& operator=(const IVoxelRenderFeature&) = delete;
    IVoxelRenderFeature& operator=(IVoxelRenderFeature&&) = delete;

    /**
     * @brief Human-readable name of the voxel structure and its construction algorithm.
     */
    virtual std::string_view get_name() const = 0;
    /**
     * @brief Human-readable name of what the debug overlay draws in a mode.
     */
    virtual const char* get_debug_view_name(VoxelDebugViewMode p_mode) const = 0;
    /**
     * @brief Registers all implementation-owned resources with their stable shader resource names.
     */
    virtual void register_render_graph_data(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Adds every construction task and the setup of the debug overlays.
     */
    virtual void add_build_render_tasks(RendGraphDescVK& p_graph, const DynamicMeshRenderFeature& p_dynamic_mesh) const = 0;
    /**
     * @brief Adds the debug overlays drawn over the presented image.
     * @details Every overlay renders into the swap chain and depth-tests against `gbuffer_depth`; get_overlay_render_task_ids()
     * lists them.
     */
    virtual void add_overlay_render_tasks(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Connects every task of the feature through resource edges on the feature's own resources.
     * @details Call after all tasks of add_build_render_tasks() and add_overlay_render_tasks() were added to p_graph. The
     * caller orders the overlays on the swap chain and `gbuffer_depth`.
     */
    virtual void connect_render_tasks(RendGraphDescVK& p_graph) const = 0;
    /**
     * @brief Task ids of the overlays added by add_overlay_render_tasks(), in drawing order.
     */
    virtual std::span<const std::string_view> get_overlay_render_task_ids() const = 0;
};

} // namespace WhiteBirdEngine

#endif
