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
#ifndef WBE_FILE_RR_TYPED_HH
#define WBE_FILE_RR_TYPED_HH

#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "function/render/rd_typed.hh"
#include "function/render/render_defs.hh"
#include "utils/utils.hh"
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class RRTyped
 * @brief A resource registrator whose descriptions are RDTyped<TParams>. Neither copyable nor movable.
 *
 * Implements the description type check of compile(); derived registrators implement compile_typed(), and implement
 * get_description_type_uuid() and deserialize_description() with typed_description_type_uuid() and
 * typed_deserialize_description() in a source file that includes the generated type_uuid and serializables headers, so
 * that including the registrator's header needs neither (deserialization is strict when TParams is
 * WBE_STRICT_DESERIALIZATION).
 *
 * @tparam TParams The parameters struct of the descriptions.
 */
template <typename TParams>
class RRTyped : public IResourceRegistrator {
public:
    virtual ~RRTyped() override = default;
    RRTyped(const RRTyped&) = delete;
    RRTyped(RRTyped&&) noexcept = delete;
    RRTyped& operator=(const RRTyped&) = delete;
    RRTyped& operator=(RRTyped&&) noexcept = delete;

    virtual Ref<IRenderObject> compile(IRenderGraph& p_graph, std::string_view p_key, const IResourceDescription& p_description) override {
        if (p_description.get_type_uuid() != get_description_type_uuid()) {
            throw std::runtime_error(std::format("Failed to register resource '{}': the description has the wrong type for its registrator.", p_key));
        }
        Ref<IRenderObject> result = compile_typed(p_graph, p_key, static_cast<const RDTyped<TParams>&>(p_description).params);
        if (result == MEM_NULL) {
            throw std::runtime_error(std::format("Failed to register resource '{}': the registrator compiled no object.", p_key));
        }
        return result;
    }

protected:
    RRTyped() = default;

    /**
     * @brief The type id of RDTyped<TParams>, for get_description_type_uuid().
     */
    static TypeID typed_description_type_uuid() {
        return TypeUUID<TParams>::UUID;
    }

    /**
     * @brief Deserialize TParams into an RDTyped<TParams>, for deserialize_description().
     */
    static Unique<IResourceDescription> typed_deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) {
        TParams params{};
        SerializableSD<TParams>::deserialize(p_description, params);
        return make_unique<RDTyped<TParams>>(p_allocator, std::move(params));
    }

    /**
     * @brief Compile the render object of a description; see IResourceRegistrator::compile().
     */
    virtual Ref<IRenderObject> compile_typed(IRenderGraph& p_graph, std::string_view p_key, const TParams& p_params) = 0;
};

} // namespace WhiteBirdEngine

#endif
