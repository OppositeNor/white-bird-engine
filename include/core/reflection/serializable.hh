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
#ifndef __WBE_SERIALIZABLE_HH__
#define __WBE_SERIALIZABLE_HH__

#include "core/parser/parser_json.hh"
#include "core/reflection/reflection_defs.hh"

#define WBE_DECL_SERIALIZABLE(class_name)\
    friend class ::WhiteBirdEngine::SerializableSD<class_name>;\
    virtual void serialize(::WhiteBirdEngine::JSONData& p_data) const override;\
    virtual void deserialize(const ::WhiteBirdEngine::JSONData& p_data) override;

namespace WhiteBirdEngine {

/**
 * @class Serializable
 * @brief Serializable struct.
 *
 */
WBE_CLASS(Serializable, WBE_SERIALIZABLE) {
public:
    Serializable() = default;
    virtual ~Serializable() {}

    /**
     * @brief Serialize.
     *
     * @param p_data The data to store serialization.
     */
    virtual void serialize(JSONData& p_data) const = 0;

    /**
     * @brief Deserialize.
     *
     * @param p_data The data to find data from.
     */
    virtual void deserialize(const JSONData& p_data) = 0;
};


}

#endif
