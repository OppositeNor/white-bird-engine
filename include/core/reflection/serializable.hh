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

namespace WhiteBirdEngine {

template <typename T>
class SerializableSD {
public:

    using ObjType = T;

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
class SerializableAccess {};

}

#endif
