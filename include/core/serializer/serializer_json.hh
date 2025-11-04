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
#ifndef __WBE_SERILIAZER_JSON_HH__
#define __WBE_SERILIAZER_JSON_HH__

#include "core/parser/parser_json.hh"
#include "serializer.hh"
#include "utils/utils.hh"
#include <cstring>
#include <glm/ext/vector_float3.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <glm/glm.hpp>

namespace WhiteBirdEngine {

/**
 * @brief Concept for types that could be serialized by nlohmann::json.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept SerializableJSON = requires (std::remove_cvref_t<T> p_t_inst, nlohmann::json p_json) {
    p_json["key"] = p_t_inst;
};

/**
 * @class SerializerJSON.
 * @brief Serializes objects to json format.
 * @todo Test
 *
 */
class SerializerJSON : public Serializer<SerializerJSON> {
    using json = nlohmann::json;
public:
    SerializerJSON() {
        context_stack.push_back(std::make_pair("base", json()));
    }
    ~SerializerJSON() {}
    SerializerJSON(const SerializerJSON&) = delete;
    SerializerJSON(SerializerJSON&&) = delete;
    SerializerJSON& operator=(const SerializerJSON&) = delete;
    SerializerJSON& operator=(SerializerJSON&&) = delete;

    void push_list(const std::string& p_key, const SerializerJSON& p_serializer) {
        context_stack.back().second[p_key].push_back(p_serializer.context_stack[0].second);
    }

    void push_context(const std::string& p_context_name) {
        context_stack.push_back(std::pair<std::string, json>(p_context_name, json()));
    }

    void pop_context() {
        if (context_stack.size() < 2) {
            throw std::runtime_error("Failed to pop context from the serializer: context stack is empty.");
        }
        auto back = std::move(context_stack.back());
        context_stack.pop_back();
        context_stack.back().second[std::move(back.first)] = std::move(back.second);
    }

    template <SerializableGLM T>
    void register_serialize(const std::string& p_key, T&& p_value) {
        using Type = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Type, glm::vec2>) {
            context_stack.back().second[p_key]["x"] = p_value.x;
            context_stack.back().second[p_key]["y"] = p_value.y;
        } else if constexpr (std::same_as<Type, glm::vec3>) {
            context_stack.back().second[p_key]["x"] = p_value.x;
            context_stack.back().second[p_key]["y"] = p_value.y;
            context_stack.back().second[p_key]["z"] = p_value.z;
        } else if constexpr (std::same_as<Type, glm::vec4>) {
            context_stack.back().second[p_key]["x"] = p_value.x;
            context_stack.back().second[p_key]["y"] = p_value.y;
            context_stack.back().second[p_key]["z"] = p_value.z;
            context_stack.back().second[p_key]["w"] = p_value.w;
        }
        else {
            static_assert(false, "Not implemented.");
        }
    }

    template <SerializableJSON T> requires (!BufferBaseConcept<T>)
    void register_serialize(const std::string& p_key, T&& p_value) {
        context_stack.back().second[p_key] = std::forward<T>(p_value);
    }

    template <BufferBaseConcept T>
    void register_serialize(const std::string& p_key, T&& p_value) {
        using BufferT = std::remove_cvref_t<T>;
        BufferT copy;
        memcpy(copy.buffer, p_value.buffer, BufferT::BUFFER_SIZE);
        // Make sure the buffer don't overflow.
        copy.buffer[BufferT::BUFFER_SIZE - 1] = '\0';
        std::string str(p_value.buffer);
        context_stack.back().second[p_key] = str;
    }

    JSONData dump_data() const {
        return JSONData(context_stack.back().second);
    }

    std::string dump() const {
        if (context_stack.size() >= 2) {
            throw std::runtime_error("Failed to dump context from the serializer: not all contexts are poped.");
        }
        return context_stack.back().second.dump();
    }

    void dump(const Path& p_path) const {
        // TODO
    }

    std::string get_current_context() const {
        return context_stack.back().first;
    }

    uint32_t get_context_depth() const {
        return context_stack.size() - 1;
    }

    /**
     * @brief Clear the serializer.
     */
    void clear() {
        context_stack.clear();
        context_stack.push_back(std::make_pair("base", json()));
    }
private:
    std::vector<std::pair<std::string, json>> context_stack;


};


}

#endif
