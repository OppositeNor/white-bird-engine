// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_SERILIAZER_JSON_HH__
#define __WBE_SERILIAZER_JSON_HH__

#include "serializer.hh"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace WhiteBirdEngine {

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

    template <SerializableJSON T>
    void register_serialize(const std::string& p_key, T&& p_value) {
        context_stack.back().second[p_key] = std::forward<T>(p_value);
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
private:
    std::vector<std::pair<std::string, json>> context_stack;


};


}

#endif
