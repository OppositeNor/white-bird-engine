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
#ifndef WBE_FILE_I_REND_OBJECT_HH
#define WBE_FILE_I_REND_OBJECT_HH

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/debug_data.hh"
#include "function/render/rend_defs.hh"
#include "utils/utils.hh"

#include <string>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class IRendObject
 * @brief Interface of the runtime rendering objects, such as buffers, images, pipelines, and render graphs. Neither copyable
 * nor movable.
 *
 * Render objects are confined to the rendering thread: their methods provide no internal synchronization, and other threads
 * submit work on them through IRenderer::add_job(). Their lifetime is bound to the renderer that created them; every
 * reference must be released before that renderer shuts down. Composite objects, which own other render objects (such as
 * a render graph and the resources it allocates), are render objects too. Objects created with make_ref() know their owning
 * Ref (acquire_this_ref()), which bindable render objects use to import themselves into render graphs.
 */
class IRendObject : public ThisRef<IRendObject> {
public:
    IRendObject() = default;
    virtual ~IRendObject() override = default;
    IRendObject(const IRendObject&) = delete;
    IRendObject(IRendObject&&) noexcept = delete;
    IRendObject& operator=(const IRendObject&) = delete;
    IRendObject& operator=(IRendObject&&) noexcept = delete;

    /**
     * @brief Get the concrete render-object type UUID.
     * @return The generated UUID of the concrete render-object type.
     */
    virtual TypeID get_type_uuid() const = 0;

    /**
     * @brief Name the backend objects the render object owns, for debuggers and validation messages.
     *
     * GlobalRendCache calls this with the key an object is registered under, so registered objects are named after their
     * database key; importing an unnamed object into a render graph names it after its import id. Backends without object
     * naming, and render objects that own no backend objects, ignore the name but still record it. Composites register
     * their parts as `<p_name>.<part>`, which names them too. Call on the rendering thread.
     *
     * @param p_name The name, normally the resource key the object is registered under.
     */
    void set_debug_name(std::string_view p_name) {
        debug_name = p_name;
        apply_debug_name(debug_name);
    }

    /**
     * @brief Return the descriptor-debug rows of the render object.
     *
     * Implementations must report only descriptors that already exist and must not allocate new descriptors while inspecting.
     * The default implementation reports no rows.
     *
     * @param p_alloc The allocator of the returned rows.
     * @return Descriptor-debug rows for this object.
     */
    virtual DebugDataRows desc_debug_data(RendAlloc* p_alloc) const {
        return DebugDataRows(p_alloc);
    }

    /**
     * @brief Get the name last given through set_debug_name().
     * @return The name, or an empty string if the object has not been named.
     */
    const std::string& get_debug_name() const {
        return debug_name;
    }

protected:
    /**
     * @brief Apply a name given through set_debug_name() to the backend objects the render object owns.
     *
     * The default implementation ignores the name.
     *
     * @param p_name The name.
     */
    virtual void apply_debug_name(std::string_view p_name) {
        static_cast<void>(p_name);
    }

private:
    std::string debug_name;
};

} // namespace WhiteBirdEngine

#endif
