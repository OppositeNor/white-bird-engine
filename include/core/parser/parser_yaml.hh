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
#ifndef __WBE_PARSER_YAML_HH__
#define __WBE_PARSER_YAML_HH__

#include "parser.hh"
#include "utils/utils.hh"
#include <format>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace WhiteBirdEngine {

/**
 * @class YAMLData
 * @brief Data class for yaml.
 *
 */
class YAMLData : public ParserData<YAMLData> {
    using yaml = YAML::Node;
    friend class ParserYAML;
    friend std::ostream& operator<<(std::ostream& p_ostrem, const YAMLData& p_parser);
public:
    YAMLData(): node(yaml()) {}
    ~YAMLData() {}
    YAMLData(const YAMLData& p_other)
        : node(p_other.node) {}
    YAMLData(YAMLData&& p_other)
        : node(std::move(p_other.node)) {}
    YAMLData& operator=(const YAMLData& p_other) {
        node = p_other.node;
        return *this;
    }
    YAMLData& operator=(YAMLData&& p_other) {
        node = std::move(p_other.node);
        return *this;
    }
    YAMLData(const yaml& p_node)
        : node(p_node) {}
    YAMLData(YAML::Node&& p_node)
        : node(std::move(p_node)) {}

    template <typename T>
    void set_value(const std::string& p_key, T&& p_value) {
        using Type = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Type, YAMLData>) {
            node[p_key] = p_value.node;
        } else if constexpr (BufferBaseConcept<T>) {
            using BufferT = std::remove_cvref_t<T>;
            std::string result = p_value.buffer;
            if (result.size() > BufferT::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format(
                    "Failed to get string value: {}. Buffer capacity: {}. String length: {} (without NUL terminator).",
                    result, BufferT::BUFFER_SIZE, result.size()));
            }
            node[p_key] = result;
        } else if constexpr (std::same_as<Type, glm::vec2>) {
            node[p_key]["x"] = p_value.x;
            node[p_key]["y"] = p_value.y;
        } else if constexpr (std::same_as<Type, glm::vec3>) {
            node[p_key]["x"] = p_value.x;
            node[p_key]["y"] = p_value.y;
            node[p_key]["z"] = p_value.z;
        } else if constexpr (std::same_as<Type, glm::vec4> || std::same_as<Type, glm::quat>) {
            node[p_key]["x"] = p_value.x;
            node[p_key]["y"] = p_value.y;
            node[p_key]["z"] = p_value.z;
            node[p_key]["w"] = p_value.w;
        } else {
            node[p_key] = std::forward<T>(p_value);
        }
    }

    /**
     * @brief Set the current parser data to a value.
     *
     * @tparam T The type of the value to set.
     * @param p_value The value to set to.
     */
    template <typename T>
    void set(T&& p_value) {
        using Type = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Type, YAMLData>) {
            node = p_value.node;
        } else if constexpr (std::same_as<Type, std::vector<YAMLData>>) {
            node = yaml();
            for (auto& item : p_value) {
                node.push_back(item.node);
            }
        } else if constexpr (BufferBaseConcept<T>) {
            using BufferT = std::remove_cvref_t<T>;
            std::string result = p_value.buffer;
            if (result.size() > BufferT::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format(
                    "Failed to get string value: {}. Buffer capacity: {}. String length: {} (without NUL terminator).",
                    result, BufferT::BUFFER_SIZE, result.size()));
            }
            node = result;
        } else if constexpr (std::same_as<Type, glm::vec2>) {
            node = yaml();
            node["x"] = p_value.x;
            node["y"] = p_value.y;
        } else if constexpr (std::same_as<Type, glm::vec3>) {
            node = yaml();
            node["x"] = p_value.x;
            node["y"] = p_value.y;
            node["z"] = p_value.z;
        } else if constexpr (std::same_as<Type, glm::vec4> || std::same_as<Type, glm::quat>) {
            node = yaml();
            node["x"] = p_value.x;
            node["y"] = p_value.y;
            node["z"] = p_value.z;
            node["w"] = p_value.w;
        } else {
            node = std::forward<T>(p_value);
        }
    }

    template <typename T>
    T get_value(const std::string& p_key) const {
        T result;
        get_value<T>(p_key, result);
        return result;
    }

    template <typename T>
    T get() const {
        T val;
        get<T>(val);
        return val;
    }


    template <typename T>
    void get(T& p_value) const {
        if constexpr (BufferBaseConcept<T>) {
            using BufferT = std::remove_cvref_t<T>;
            std::string result = node.as<std::string>();
            if (result.size() > BufferT::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format(
                    "Failed to get string value: {}. Buffer capacity: {}. String length: {} (without NUL terminator).",
                    result, BufferT::BUFFER_SIZE, result.size()));
            }
            strncpy(p_value.buffer, result.data(), BufferT::BUFFER_SIZE - 1);
            p_value.buffer[BufferT::BUFFER_SIZE - 1] = '\0';
        } else if constexpr (std::same_as<T, std::vector<YAMLData>>) {
            p_value.clear();
            for (const auto& elem : node) {
                p_value.emplace_back(elem);
            }
        } else if constexpr (std::same_as<T, glm::vec2>) {
            p_value = glm::vec2(
                node["x"].as<float>(),
                node["y"].as<float>());
        } else if constexpr (std::same_as<T, glm::vec3>) {
            p_value = glm::vec3(
                node["x"].as<float>(),
                node["y"].as<float>(),
                node["z"].as<float>());
        } else if constexpr (std::same_as<T, glm::vec4>) {
            p_value = glm::vec4(
                node["x"].as<float>(),
                node["y"].as<float>(),
                node["z"].as<float>(),
                node["w"].as<float>());
        } else if constexpr (std::same_as<T, glm::quat>) {
            p_value = glm::quat(
                node["x"].as<float>(),
                node["y"].as<float>(),
                node["z"].as<float>(),
                node["w"].as<float>());
        } else if constexpr (std::same_as<T, YAMLData>) {
            p_value = YAMLData(node.as<yaml>());
        } else {
            p_value = node.as<T>();
        }
    }

    template <typename T>
    void get_value(const std::string& p_key, T& p_value) const {
        if constexpr (BufferBaseConcept<T>) {
            using BufferT = std::remove_cvref_t<T>;
            std::string result = node[p_key].as<std::string>();
            if (result.size() > BufferT::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format(
                    "Failed to get string value: {}. Buffer capacity: {}. String length: {} (without NUL terminator).",
                    result, BufferT::BUFFER_SIZE, result.size()));
            }
            strncpy(p_value.buffer, result.data(), BufferT::BUFFER_SIZE - 1);
            p_value.buffer[BufferT::BUFFER_SIZE - 1] = '\0';
        } else if constexpr (std::same_as<T, std::vector<YAMLData>>) {
            std::vector<YAMLData> result;
            for (const auto& elem : node[p_key]) {
                result.emplace_back(elem);
            }
            p_value = result;
        } else if constexpr (std::same_as<T, glm::vec2>) {
            p_value = glm::vec2(
                node[p_key]["x"].as<float>(),
                node[p_key]["y"].as<float>());
        } else if constexpr (std::same_as<T, glm::vec3>) {
            p_value = glm::vec3(
                node[p_key]["x"].as<float>(),
                node[p_key]["y"].as<float>(),
                node[p_key]["z"].as<float>());
        } else if constexpr (std::same_as<T, glm::vec4>) {
            p_value = glm::vec4(
                node[p_key]["x"].as<float>(),
                node[p_key]["y"].as<float>(),
                node[p_key]["z"].as<float>(),
                node[p_key]["w"].as<float>());
        } else if constexpr (std::same_as<T, YAMLData>) {
            p_value = YAMLData(node[p_key].as<yaml>());
        } else {
            p_value = node[p_key].as<T>();
        }
    }

    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> result;
        result.reserve(node.size());
        for (yaml::const_iterator it = node.begin(); it != node.end(); ++it) {
            result.push_back(it->first.as<std::string>());
        }
        return result;
    }

    bool contains(const std::string& p_key) const {
        return node[p_key].IsDefined();
    }

private:
    yaml node;
};

inline std::ostream& operator<<(std::ostream& p_ostream, const YAMLData& p_parser) {
    return p_ostream << p_parser.node;
}

/**
 * @class ParserYAML
 * @brief Parser for YAML files.
 * @todo Test
 */
class ParserYAML : public Parser<ParserYAML> {
    using yaml = YAML::Node;
public:

    using DataType = YAMLData;

    ParserYAML() = default;
    virtual ~ParserYAML() {}
    ParserYAML(const ParserYAML& p_other)
        : data(p_other.data) {}
    ParserYAML(ParserYAML&& p_other)
        : data(std::move(p_other.data) ){}
    ParserYAML& operator=(const ParserYAML& p_other) {
        data = p_other.data;
        return *this;
    }
    ParserYAML& operator=(ParserYAML&& p_other) {
        data = std::move(p_other.data);
        return *this;
    }

    void parse(const Path& p_path) {
        data.node = YAML::LoadFile(static_cast<std::string>(p_path));
    }

    void parse_from_buffer(const std::string& p_buffer) {
        data.node = YAML::Load(p_buffer);
    }

    template <typename T>
    T get_value(const std::string& p_key) const {
        return data.get_value<T>(p_key);
    }

    template <typename T>
    void get_value(const std::string& p_key, T& p_val) const {
        data.get_value<T>(p_key, p_val);
    }

    template <typename T>
    T get() const {
        return data.get_value<T>();
    }
    template <typename T>
    void get(T& p_val) const {
        data.get_value<T>(p_val);
    }

    std::vector<std::string> get_all_keys() const {
        return data.get_all_keys();
    }

    const YAMLData& get_data() const {
        return data;
    }

    YAMLData& get_data() {
        return data;
    }

    bool contains(const std::string& p_key) const {
        return data.contains(p_key);
    }

    template <typename T>
    void get_value(std::string& p_key, T&& p_value) {
        return data.get_value<T>(std::forward<T>(p_value));
    }

private:
    YAMLData data;
};

}

#endif
