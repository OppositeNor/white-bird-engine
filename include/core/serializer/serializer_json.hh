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

#include "serializer.hh"
#include "utils/utils.hh"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

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

    template <SerializableJSON T> requires (!BufferBaseConcept<T>)
    void register_serialize(const std::string& p_key, T&& p_value) {
        context_stack.back().second[p_key] = std::forward<T>(p_value);
    }

    template <BufferBaseConcept T>
    void register_serialize(const std::string& p_key, T&& p_value) {
        using BufferT = std::remove_cvref_t<T>;
        // Make sure the buffer don't overflow.
        p_value.buffer[BufferT::BUFFER_SIZE - 1] = '\0';
        std::string str(p_value.buffer);
        context_stack.back().second[p_key] = str;
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
