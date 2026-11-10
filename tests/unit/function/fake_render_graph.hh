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
#ifndef WBE_FILE_FAKE_RENDER_GRAPH_HH
#define WBE_FILE_FAKE_RENDER_GRAPH_HH

#include "core/allocator/i_allocator.hh"
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_graph_bindings.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_graph_resource_registry.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class FakeRenderGraph
 * @brief An IRenderGraph without a backend, for tests of registration and of task types that resolve registered resources.
 * Neither copyable nor movable.
 *
 * Registrations go to a RenderGraphResourceRegistry without a release callback, so replaced objects are released at once.
 * rebuild() only counts its calls. A reflected render object, so GlobalRenderCache can cache it.
 */
class WBE_META(WBE_RENDER_OBJECT) FakeRenderGraph final : public IRenderGraph {
public:
    /**
     * @brief Construct a graph with no registrators.
     * @param p_allocator The allocator of the registry.
     * @param p_bindings The bindings get_bindings() returns.
     */
    explicit FakeRenderGraph(RenderAllocator* p_allocator, Ref<IRenderGraphBindings> p_bindings = MEM_NULL)
        : registry(p_allocator, this), bindings(std::move(p_bindings)) {
    }

    virtual TypeID get_type_uuid() const override {
        // The generated TypeUUID<FakeRenderGraph> includes this header, so the UUID is computed as the generator does.
        return static_hash("FakeRenderGraph");
    }

    /**
     * @brief Get the bindings given at construction.
     */
    Ref<IRenderGraphBindings> get_bindings() const {
        return bindings;
    }

    virtual void add_resource_registrator(Unique<IResourceRegistrator> p_registrator) override {
        registry.add_registrator(std::move(p_registrator));
    }

    virtual bool has_resource_registrator(TypeID p_description_type) const override {
        return registry.has_registrator(p_description_type);
    }

    virtual Ref<IRenderObject> register_resource(std::string_view p_key, const IResourceDescription& p_description) override {
        return registry.register_resource(p_key, p_description);
    }

    virtual Ref<IRenderObject> register_resource(std::string_view p_key, TypeID p_description_type, const DataValue& p_description) override {
        return registry.register_resource(p_key, p_description_type, p_description);
    }

    virtual void unregister_resource(std::string_view p_key) override {
        registry.unregister_resource(p_key);
    }

    virtual bool contains_registered_resource(std::string_view p_key) const override {
        return registry.contains(p_key);
    }

    virtual Ref<IRenderObject> try_acquire_registered_resource(std::string_view p_key) const override {
        return registry.try_get(p_key);
    }

    virtual void rebuild() override {
        ++rebuild_count;
    }

    /**
     * @brief The number of rebuild() calls.
     */
    size_t rebuild_count = 0;

private:
    RenderGraphResourceRegistry registry;
    Ref<IRenderGraphBindings> bindings;
};

} // namespace WhiteBirdEngine

#endif
