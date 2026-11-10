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
#ifndef WBE_FILE_RENDER_GRAPH_INSTANCE_CACHE_HH
#define WBE_FILE_RENDER_GRAPH_INSTANCE_CACHE_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_render_cache.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_graph_bindings.hh"
#include <concepts>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class RenderGraphInstanceCache
 * @brief The identity rules of render graph instances, applied to the renderer's GlobalRenderCache. Stateless.
 *
 * An instance is identified by its definition's resource key and its bindings' instance key, which make_cache_key() joins
 * into one unambiguous cache key. The cache holds instances weakly: applications, scenes, and the renderer's activation
 * hold them strongly, and an instance whose last owner released it is compiled again by the next acquisition. Used by
 * renderers on the rendering thread.
 */
class RenderGraphInstanceCache final {
public:
    RenderGraphInstanceCache() = delete;
    ~RenderGraphInstanceCache() = delete;
    RenderGraphInstanceCache(const RenderGraphInstanceCache&) = delete;
    RenderGraphInstanceCache(RenderGraphInstanceCache&&) noexcept = delete;
    RenderGraphInstanceCache& operator=(const RenderGraphInstanceCache&) = delete;
    RenderGraphInstanceCache& operator=(RenderGraphInstanceCache&&) noexcept = delete;

    /**
     * @brief Join a definition key and an instance key into the cache key of their instance.
     *
     * The definition key is length-prefixed, so no two pairs share a cache key.
     *
     * @param p_definition_key The resource key of the render graph definition.
     * @param p_instance_key The instance key of the bindings.
     * @return The cache key.
     */
    static std::string make_cache_key(std::string_view p_definition_key, std::string_view p_instance_key) {
        return std::format("render_graph:{}:{}:{}", p_definition_key.size(), p_definition_key, p_instance_key);
    }

    /**
     * @brief Get the live instance of a definition for some bindings, or compile and cache a new one.
     *
     * A live instance with the same definition and instance key is returned when it was acquired with the same bindings
     * object. Otherwise p_factory compiles the instance, which is cached only after the factory returns it; a factory that
     * throws leaves the cache unchanged.
     *
     * @param p_cache The renderer's cache.
     * @param p_definition_key The resource key of the render graph definition.
     * @param p_bindings The bindings; their instance key identifies the instance.
     * @param p_factory Called with no arguments on a miss; returns the compiled, non-null Ref<TGraph>.
     * @return The live or compiled instance; TGraph::get_bindings() returns p_bindings.
     * @throws std::runtime_error If the definition key is empty, the bindings are null, a live instance with the same
     * identity was acquired with another bindings object, the factory returns null, or whatever the factory throws.
     */
    template <typename TGraph, typename TFactory>
        requires std::derived_from<TGraph, IRenderGraph>
    static Ref<TGraph> acquire(
        GlobalRenderCache& p_cache, std::string_view p_definition_key, const Ref<IRenderGraphBindings>& p_bindings, TFactory&& p_factory) {
        if (p_definition_key.empty()) {
            throw std::runtime_error("Failed to acquire a render graph: the definition key is empty.");
        }
        if (p_bindings == MEM_NULL) {
            throw std::runtime_error(std::format("Failed to acquire render graph '{}': the bindings are null.", p_definition_key));
        }
        const std::string cache_key = make_cache_key(p_definition_key, p_bindings->get_instance_key());
        Ref<TGraph> live = p_cache.try_get<TGraph>(cache_key);
        if (live != MEM_NULL) {
            if (live->get_bindings() != p_bindings) {
                throw std::runtime_error(std::format("Failed to acquire render graph '{}' instance '{}': the live instance was acquired with another bindings object.",
                    p_definition_key,
                    p_bindings->get_instance_key()));
            }
            return live;
        }
        Ref<TGraph> graph = std::forward<TFactory>(p_factory)();
        if (graph == MEM_NULL) {
            throw std::runtime_error(std::format("Failed to acquire render graph '{}': the factory returned null.", p_definition_key));
        }
        p_cache.insert(cache_key, graph);
        return graph;
    }
};

} // namespace WhiteBirdEngine

#endif
