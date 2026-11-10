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
#ifndef WBE_FILE_I_RENDER_GRAPH_BINDINGS_HH
#define WBE_FILE_I_RENDER_GRAPH_BINDINGS_HH

#include <string>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class IRenderGraphBindings
 * @brief The runtime half of a render graph instance: what an application supplies to a render graph definition (RURenderGraph)
 * when it acquires a graph from it (IRenderer::acquire_render_graph()). Neither copyable nor movable.
 *
 * A definition names a recipe and its static parameters; its bindings supply the runtime resources the recipe imports and the
 * native tasks it imports, and the recipe owns everything else (topology, registrations, task bindings, triggers, and toggles).
 * Bindings never see or return the graph's descriptor and never receive the graph itself. Each backend declares the
 * operations its bindings implement (see IRenderGraphBindingsVK).
 *
 * The instance key identifies the instance among the instances of a definition: acquiring a definition again with the same
 * instance key and the same bindings object returns the same live graph, while different instance keys compile independent
 * graphs. The key is immutable. A compiled graph retains its bindings strongly, so bindings must not retain their graph, and
 * the objects they borrow must outlive the graph's use. Bindings are used on the rendering thread.
 */
class IRenderGraphBindings {
public:
    IRenderGraphBindings() = delete;
    virtual ~IRenderGraphBindings() = default;
    IRenderGraphBindings(const IRenderGraphBindings&) = delete;
    IRenderGraphBindings(IRenderGraphBindings&&) noexcept = delete;
    IRenderGraphBindings& operator=(const IRenderGraphBindings&) = delete;
    IRenderGraphBindings& operator=(IRenderGraphBindings&&) noexcept = delete;

    /**
     * @brief Get the key of the instance these bindings belong to.
     * @return The non-empty instance key.
     */
    std::string_view get_instance_key() const {
        return instance_key;
    }

protected:
    /**
     * @brief Construct bindings for an instance.
     * @param p_instance_key The instance key; usually the definition's resource key when a definition has one instance.
     * @throws std::runtime_error If p_instance_key is empty.
     */
    explicit IRenderGraphBindings(std::string_view p_instance_key);

private:
    std::string instance_key;
};

} // namespace WhiteBirdEngine

#endif
