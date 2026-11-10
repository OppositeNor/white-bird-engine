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
#ifndef WBE_FILE_I_RENDER_GRAPH_HH
#define WBE_FILE_I_RENDER_GRAPH_HH

#include "core/allocator/i_allocator.hh"
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "utils/utils.hh"
#include <format>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class IRenderGraph
 * @brief A backend-neutral render graph, and the owner of the named resources its tasks use. Neither copyable nor movable.
 *
 * A render graph is a composite render object (IRenderObject): it is confined to the rendering thread, and its lifetime is
 * bound to the renderer that compiled it (IRenderer::acquire_render_graph()). Backends implement get_type_uuid() with the
 * concrete graph type's UUID, so graphs can be cached in GlobalRenderCache like other render objects.
 *
 * Applications register the resources a graph's tasks name (meshes, images, pipelines, ...) on the graph itself, in two
 * steps: load the source resources from the ResourceManager, then register_resource() with a description. The description's
 * type (IResourceDescription::get_type_uuid()) selects the IResourceRegistrator that compiles it, as a resource schema's
 * type selects its resource unit loader; backends install default registrators for the common description types, and add_resource_registrator()
 * adds custom ones. The graph holds every registered object strongly until it is unregistered, replaced, or the graph is
 * destroyed, and registrations survive rebuilds. Registering a key again with an equal description returns the existing
 * object without compiling, so a recipe may register on every rebuild. Registration keys are local to the graph.
 */
class IRenderGraph : public IRenderObject {
public:
    virtual ~IRenderGraph() override = default;
    IRenderGraph(const IRenderGraph&) = delete;
    IRenderGraph(IRenderGraph&&) noexcept = delete;
    IRenderGraph& operator=(const IRenderGraph&) = delete;
    IRenderGraph& operator=(IRenderGraph&&) noexcept = delete;

    /**
     * @brief Install a registrator for the description type it accepts (IResourceRegistrator::get_description_type_uuid()).
     *
     * @param p_registrator The registrator; the graph owns it.
     * @throws std::runtime_error If the registrator is null, or its description type already has a registrator.
     */
    virtual void add_resource_registrator(Unique<IResourceRegistrator> p_registrator) = 0;

    /**
     * @brief Check whether a description type has a registrator.
     */
    virtual bool has_resource_registrator(TypeID p_description_type) const = 0;

    /**
     * @brief Register a resource under a key, compiling it with the registrator of the description's type.
     *
     * When the key is registered with an equal description, the existing object is returned and nothing is compiled.
     * Otherwise the description is compiled and replaces any earlier registration of the key, of any type; the replaced
     * object is released once the frames that may use it have completed.
     *
     * @param p_key The registration key, unique in the graph.
     * @param p_description The description; the graph keeps a copy.
     * @return The registered object.
     * @throws std::runtime_error If the key is empty, the description type has no registrator, or compiling fails.
     */
    virtual Ref<IRenderObject> register_resource(std::string_view p_key, const IResourceDescription& p_description) = 0;

    /**
     * @brief Register a resource from a description written by a script; see
     * register_resource(std::string_view, const IResourceDescription&).
     *
     * @param p_key The registration key.
     * @param p_description_type The type of the description, which selects the registrator that deserializes it.
     * @param p_description The serialized description.
     * @throws std::runtime_error Under the same conditions, or if the registrator rejects the serialized description.
     */
    virtual Ref<IRenderObject> register_resource(std::string_view p_key, TypeID p_description_type, const DataValue& p_description) = 0;

    /**
     * @brief Remove a registration; its object is released once the frames that may use it have completed.
     *
     * @throws std::runtime_error If the key is not registered.
     */
    virtual void unregister_resource(std::string_view p_key) = 0;

    /**
     * @brief Check whether a key is registered.
     */
    virtual bool contains_registered_resource(std::string_view p_key) const = 0;

    /**
     * @brief Get the object registered under a key.
     *
     * @return The object, or null when the key is not registered.
     */
    virtual Ref<IRenderObject> try_acquire_registered_resource(std::string_view p_key) const = 0;

    /**
     * @brief Get the object registered under a key.
     *
     * @throws std::runtime_error If the key is not registered.
     */
    Ref<IRenderObject> acquire_registered_resource(std::string_view p_key) const {
        Ref<IRenderObject> result = try_acquire_registered_resource(p_key);
        if (result == MEM_NULL) {
            throw std::runtime_error(std::format("No resource is registered on the render graph under '{}'.", p_key));
        }
        return result;
    }

    /**
     * @brief Get the object of type T registered under a key.
     *
     * @return The object, or null when the key is not registered.
     * @throws std::runtime_error If the registered object is not a T.
     */
    template <typename T>
    Ref<T> try_acquire_registered(std::string_view p_key) const {
        Ref<IRenderObject> object = try_acquire_registered_resource(p_key);
        if (object == MEM_NULL) {
            return MEM_NULL;
        }
        Ref<T> result = object.template dynamic_cast_ref<T>();
        if (result == MEM_NULL) {
            throw std::runtime_error(std::format("The resource registered on the render graph under '{}' has another type.", p_key));
        }
        return result;
    }

    /**
     * @brief Get the object of type T registered under a key.
     *
     * @throws std::runtime_error If the key is not registered or its object is not a T.
     */
    template <typename T>
    Ref<T> acquire_registered(std::string_view p_key) const {
        Ref<T> result = try_acquire_registered<T>(p_key);
        if (result == MEM_NULL) {
            throw std::runtime_error(std::format("No resource is registered on the render graph under '{}'.", p_key));
        }
        return result;
    }

    /**
     * @brief Rebuild the graph against its current context, compiling its definition again.
     *
     * Use it after state that the graph's bindings or recipe read has changed structurally. Must be called on the rendering
     * thread while none of the graph's submitted work is pending.
     */
    virtual void rebuild() = 0;

protected:
    IRenderGraph() = default;
};

} // namespace WhiteBirdEngine

#endif
