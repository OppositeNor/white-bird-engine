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
#ifndef __WBE_OBJECT_HH__
#define __WBE_OBJECT_HH__

#include <string_view>
#include "core/reflection/serializable.hh"
#include "core/reflection/reflection_defs.hh"
#include "utils/utils.hh"
#include <string_view>

#define WBE_DECL_OBJECT(class_name)\
    WBE_DECL_SERIALIZABLE(class_name)\
    virtual std::string_view get_type() const override {\
        return #class_name;\
    }\
    virtual TypeID get_type_id() const override {\
        return static_hash(#class_name);\
    }\



namespace WhiteBirdEngine {

/**
 * @class Object
 * @brief Object class.
 *
 */
WBE_CLASS(Object, WBE_SERIALIZABLE) : public Serializable {
public:
    WBE_DECL_SERIALIZABLE(Object)

    Object() = default;
    virtual ~Object() {}

    /**
     * @brief Get the type of the object in string.
     * 
     * @return The type of the object in string.
     */
    virtual std::string_view get_type() const = 0;

    /**
     * @brief Get the type ID of the object.
     *
     * @return The type ID of the object.
     */
    virtual TypeID get_type_id() const = 0;

};

}

#endif
