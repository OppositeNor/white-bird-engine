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
#ifndef WBE_FILE_I_RENDER_OBJECT_HH
#define WBE_FILE_I_RENDER_OBJECT_HH

#include "utils/utils.hh"

#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class IRenderObject
 * @brief Interface for all the render objects, such as pipelines, shaders, etc.
 *
 */
class IRenderObject {
public:
    IRenderObject() = default;
    virtual ~IRenderObject() = default;
    IRenderObject(const IRenderObject&) = delete;
    IRenderObject(IRenderObject&&) noexcept = delete;
    IRenderObject& operator=(const IRenderObject&) = delete;
    IRenderObject& operator=(IRenderObject&&) noexcept = delete;

    /**
     * @brief Get the concrete render-object type UUID.
     * @return The generated UUID of the concrete render-object type.
     */
    virtual TypeID get_type_uuid() const = 0;

    /**
     * @brief Name the backend objects the render object owns, for debuggers and validation messages.
     *
     * RenderDatabase calls this with the key an object is registered under, so registered objects are named after their
     * database key. Backends without object naming, and render objects that own no backend objects, ignore the call.
     * Composites register their parts as `<p_name>.<part>`, which names them too. Call on the rendering thread.
     *
     * @param p_name The name, normally the resource key the object is registered under.
     */
    virtual void set_debug_name(std::string_view p_name) {
        static_cast<void>(p_name);
    }
};

} // namespace WhiteBirdEngine

#endif
