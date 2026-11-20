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
#ifndef WBE_FILE_I_REND_OBJECT_CONTAINER_HH
#define WBE_FILE_I_REND_OBJECT_CONTAINER_HH

#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/i_rend_task.hh"
#include <cstdint>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @class IRendObjectContainer
 * @brief Interface for stable resource bindings that resolve one render object at use time.
 */
class IRendObjectContainer {
public:
    IRendObjectContainer() = default;
    virtual ~IRendObjectContainer() = default;
    IRendObjectContainer(const IRendObjectContainer&) = delete;
    IRendObjectContainer(IRendObjectContainer&&) noexcept = delete;
    IRendObjectContainer& operator=(const IRendObjectContainer&) = delete;
    IRendObjectContainer& operator=(IRendObjectContainer&&) noexcept = delete;

    virtual Ref<IRendObject> get() = 0;
    virtual Ref<IRendObject> get() const = 0;
    virtual Ref<IRendObject> get(uint32_t p_frame_slot) = 0;
    virtual Ref<IRendObject> get(uint32_t p_frame_slot) const = 0;

    RendResTID get_resource_type_id() const {
        return resource_type_id;
    }

protected:
    void retain_resource_type(Ref<IRendObject> p_resource) {
        if (p_resource == MEM_NULL) {
            return;
        }
        RendResTID concrete_type_id = p_resource->get_type_uuid();
        if (resource_type_id == 0) {
            resource_type_id = concrete_type_id;
            return;
        }
        if (resource_type_id != concrete_type_id) {
            throw std::runtime_error("Render object container resources must have the same concrete type.");
        }
    }

    RendResTID resource_type_id = 0;
};

} // namespace WhiteBirdEngine

#endif
