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
#ifndef WBE_FILE_I_RESOURCE_REGISTRATOR_HH
#define WBE_FILE_I_RESOURCE_REGISTRATOR_HH

#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/render_defs.hh"
#include "utils/utils.hh"
#include <string_view>

namespace WhiteBirdEngine {

class IRenderGraph;

/**
 * @class IResourceRegistrator
 * @brief Compiles the resources of one description type registered on a render graph. Neither copyable nor movable.
 *
 * A render graph maps each description type to one registrator (IRenderGraph::add_resource_registrator()), the way the
 * ResourceManager maps a resource schema type to its loader. The registrator owns the compile step: loading the source resources its
 * description names, compiling them (through the GlobalRenderCache, so graphs share compiled objects), and returning the
 * object the graph keeps under the registration key. Called on the rendering thread.
 */
class IResourceRegistrator {
public:
    virtual ~IResourceRegistrator() = default;
    IResourceRegistrator(const IResourceRegistrator&) = delete;
    IResourceRegistrator(IResourceRegistrator&&) noexcept = delete;
    IResourceRegistrator& operator=(const IResourceRegistrator&) = delete;
    IResourceRegistrator& operator=(IResourceRegistrator&&) noexcept = delete;

    /**
     * @brief Get the type id of the descriptions this registrator accepts (IResourceDescription::get_type_uuid()).
     */
    virtual TypeID get_description_type_uuid() const = 0;

    /**
     * @brief Deserialize a description written by a script.
     *
     * @param p_description The serialized parameters.
     * @param p_allocator The allocator of the description.
     * @return The description.
     * @throws std::runtime_error If p_description is not a valid description of this registrator's type.
     */
    virtual Unique<IResourceDescription> deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) const = 0;

    /**
     * @brief Compile the render object a description describes.
     *
     * @param p_graph The graph the resource is registered on; registrators may look up its other registrations.
     * @param p_key The registration key.
     * @param p_description The description; its type is get_description_type_uuid().
     * @return The compiled object, never null.
     * @throws std::runtime_error If the description has another type, a source resource is missing, or compiling fails.
     */
    virtual Ref<IRenderObject> compile(IRenderGraph& p_graph, std::string_view p_key, const IResourceDescription& p_description) = 0;

protected:
    IResourceRegistrator() = default;
};

} // namespace WhiteBirdEngine

#endif
