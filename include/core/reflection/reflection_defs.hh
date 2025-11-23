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
#ifndef __WBE_REFLECTION_HH__
#define __WBE_REFLECTION_HH__

#include "utils/defs.hh"

#ifdef WBE_REFLECTION_PARSER
#define WBE_ATTR(...) __attribute__((annotate(#__VA_ARGS__)))
#define WBE_META(...) WBE_ATTR(WBE, __VA_ARGS__)
#define WBE_LABEL(label_name, ...) constexpr HashCode WBE_META(__VA_ARGS__) label_name = static_hash(WBE_EXPAND_STR(label_name));

#else
#define WBE_META(...)
#define WBE_LABEL(label_name, ...) constexpr HashCode label_name = static_hash(WBE_EXPAND_STR(label_name));

#endif

namespace WhiteBirdEngine {
/**
 * @class SerializableSD
 * @brief Serializer/deserializer for genenral types.
 * @tparam T The type to serialize/deserialize.
 */
template <typename T>
class SerializableSD {
public:

    using ObjType = T;

    /**
     * @brief Serialize.
     *
     * @param p_key The key to insert into p_data.
     * @param p_data The data to insert result.
     * @param p_serializable The object to serialize.
     */
    template <typename ParserDataType>
    static void serialize(ParserDataType& p_data, const ObjType& p_serializable) {
        p_data.set(p_serializable);
    }

    /**
     * @brief Deserialize.
     *
     * @param p_serializable The object to deserialize.
     */
    template <typename ParserDataType>
    static void deserialize(const ParserDataType& p_data, ObjType& p_serializable) {
         p_data.template get<ObjType>(p_serializable);
    }

};

}

#endif
