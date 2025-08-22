// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_SERIALIZER_YAML_HH__
#define __WBE_SERIALIZER_YAML_HH__

#include "serializer.hh"

#include <fstream>
#include <sstream>
#include <string>
#include <yaml-cpp/yaml.h>

template <typename T>
concept SerializableYAML = requires (std::remove_cvref_t<T> p_t_inst, YAML::Node p_yaml) {
    p_yaml["key"] = p_t_inst;
};

namespace WhiteBirdEngine {

/**
 * @class SerializerYAML.
 * @brief Serializes objects to yaml format.
 * @todo Test
 *
 */
class SerializerYAML : public Serializer<SerializerYAML> {
    using yaml = YAML::Node;
public:
    SerializerYAML() {
        context_stack.push_back(std::make_pair("base", yaml()));
    }
    ~SerializerYAML() {}
    SerializerYAML(const SerializerYAML&) = delete;
    SerializerYAML(SerializerYAML&&) = delete;
    SerializerYAML& operator=(const SerializerYAML&) = delete;
    SerializerYAML& operator=(SerializerYAML&&) = delete;

    void push_context(const std::string& p_context_name) {
        context_stack.push_back(std::pair<std::string, yaml>(p_context_name, yaml()));
    }

    void pop_context() {
        if (context_stack.size() < 2) {
            throw std::runtime_error("Failed to pop context from the serializer: context stack is empty.");
        }
        auto back = std::move(context_stack.back());
        context_stack.pop_back();
        context_stack.back().second[std::move(back.first)] = std::move(back.second);
    }

    template <SerializableYAML T>
    void register_serialize(const std::string& p_key, T&& p_value) {
        context_stack.back().second[p_key] = std::forward<T>(p_value);
    }

    std::string dump() const {
        if (context_stack.size() >= 2) {
            throw std::runtime_error("Failed to dump context from the serializer: not all contexts are poped.");
        }
        std::stringstream ss;
        ss << context_stack.back().second;
        return ss.str();
    }

    void dump(const Path& p_path) const {
        if (context_stack.size() >= 2) {
            throw std::runtime_error("Failed to dump context from the serializer: not all contexts are poped.");
        }
        std::ofstream f(static_cast<std::string>(p_path));
        f << context_stack.back().second;
    }

    std::string get_current_context() const {
        return context_stack.back().first;
    }

    uint32_t get_context_depth() const {
        return context_stack.size() - 1;
    }

private:
    std::vector<std::pair<std::string, yaml>> context_stack;
};

}

#endif
