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
#ifndef WBE_FILE_I_RESOURCE_DESCRIPTION_HH
#define WBE_FILE_I_RESOURCE_DESCRIPTION_HH

#include "core/data/data_value.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

/**
 * @class IResourceDescription
 * @brief Describes one resource to register on a render graph (IRenderGraph::register_resource()).
 *
 * A description is a value: it names the source resources (by ResourceManager key) and the compile options, and the
 * resource registrator of its kind turns it into a render object. Two descriptions that compare equal compile to
 * interchangeable objects, which is how a graph recognizes a repeated registration. Copied only through clone().
 */
class IResourceDescription {
public:
    virtual ~IResourceDescription() = default;
    IResourceDescription& operator=(const IResourceDescription&) = delete;
    IResourceDescription& operator=(IResourceDescription&&) noexcept = delete;

    /**
     * @brief Get the type id of the description's parameters, which identifies the description type.
     */
    virtual TypeID get_type_uuid() const = 0;

    /**
     * @brief Check whether another description has the same type and parameters.
     */
    virtual bool equals(const IResourceDescription& p_other) const = 0;

    /**
     * @brief Copy the description.
     *
     * @param p_allocator The allocator of the copy.
     * @return The copy.
     */
    virtual Unique<IResourceDescription> clone(RenderAllocator* p_allocator) const = 0;

    /**
     * @brief Serialize the description's parameters, as a script would write them.
     */
    virtual DataValue serialize() const = 0;

protected:
    IResourceDescription() = default;
    IResourceDescription(const IResourceDescription&) = default;
    IResourceDescription(IResourceDescription&&) noexcept = default;
};

} // namespace WhiteBirdEngine

#endif
