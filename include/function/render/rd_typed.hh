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
#ifndef WBE_FILE_RD_TYPED_HH
#define WBE_FILE_RD_TYPED_HH

#include "core/data/data_value.hh"
#include "core/memory/unique.hh"
#include "core/reflection/serializable_sd.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/render_defs.hh"
#include "utils/utils.hh"
#include <utility>

namespace WhiteBirdEngine {

template <typename T>
struct TypeUUID;

/**
 * @class RDTyped
 * @brief A resource description holding a reflected parameters struct. Copyable and movable.
 *
 * TParams is a static serializable (WBE_SERIALIZABLE_STATIC, usually also WBE_STRICT_DESERIALIZATION and
 * WBE_REFLECT_SCRIPT so recipes can write it) with a complete operator==. Reflection does not let such a struct derive
 * from IResourceDescription, so the description holds it instead. Instantiate where the generated type_uuid and
 * serializables headers are included.
 *
 * @tparam TParams The parameters struct.
 */
template <typename TParams>
class RDTyped final : public IResourceDescription {
public:
    RDTyped() = default;
    virtual ~RDTyped() override = default;
    RDTyped(const RDTyped&) = default;
    RDTyped(RDTyped&&) noexcept = default;
    RDTyped& operator=(const RDTyped&) = delete;
    RDTyped& operator=(RDTyped&&) noexcept = delete;

    /**
     * @brief Construct a description from its parameters.
     */
    explicit RDTyped(TParams p_params) : params(std::move(p_params)) {
    }

    virtual TypeID get_type_uuid() const override {
        return TypeUUID<TParams>::UUID;
    }

    virtual bool equals(const IResourceDescription& p_other) const override {
        if (p_other.get_type_uuid() != get_type_uuid()) {
            return false;
        }
        return static_cast<const RDTyped&>(p_other).params == params;
    }

    virtual Unique<IResourceDescription> clone(RenderAllocator* p_allocator) const override {
        return make_unique<RDTyped>(p_allocator, params);
    }

    virtual DataValue serialize() const override {
        DataValue result;
        SerializableSD<TParams>::serialize(result, params);
        return result;
    }

    /**
     * @brief The parameters.
     */
    TParams params;
};

} // namespace WhiteBirdEngine

#endif
