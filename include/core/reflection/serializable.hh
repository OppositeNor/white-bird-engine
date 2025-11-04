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

#include <vector>
namespace WhiteBirdEngine {

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
    template <typename ParserData>
    static void serialize(ParserData& p_data, const ObjType& p_serializable) {
        p_data.set(p_serializable);
    }

    /**
     * @brief Deserialize.
     *
     * @param p_serializable The object to deserialize.
     */
    template <typename ParserData>
    static void deserialize(const ParserData& p_data, ObjType& p_serializable) {
         p_data.template get<ObjType>(p_serializable);
    }

};

template <typename T>
class SerializableSD<std::vector<T>> {
public:

    using ObjType = std::vector<T>;

    /**
     * @brief Serialize.
     *
     * @param p_key The key to insert into p_data.
     * @param p_data The data to insert result.
     * @param p_serializable The object to serialize.
     */
    template <typename ParserData>
    static void serialize(ParserData& p_data, const ObjType& p_serializable) {
        std::vector<ParserData> result;
        result.reserve(p_serializable.size());
        for (auto& val : p_serializable) {
            ParserData val_data;
            SerializableSD<T>::serialize(val_data, val);
            result.push_back(val_data);
        }
        p_data.set(result);
    }

    /**
     * @brief Deserialize.
     *
     * @param p_serializable The object to deserialize.
     */
    template <typename ParserData>
    static void deserialize(const ParserData& p_data, ObjType& p_serializable) {
        p_serializable.clear();
        for (auto& data : p_data) {
            T result;
            SerializableSD<T>::deserialize(data, result);
            p_serializable.push_back(result);
        }
    }

};

template <typename T>
class SerializableAccess {};

}

#endif
