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
#include <format>
#include <yaml-cpp/yaml.h>

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
    YAMLData() {}
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
    T get_value(const std::string& p_key) const {
        if constexpr (std::same_as<T, std::vector<YAMLData>>) {
            std::vector<YAMLData> result;
            for (const auto& elem : node[p_key]) {
                result.emplace_back(elem);
            }
            return result;
        } else if constexpr (!std::same_as<T, YAMLData>) {
            return node[p_key].as<T>();
        } else {
            return YAMLData(node[p_key].as<yaml>());
        }
    }

    template <typename T>
    T get_value() const {
        if constexpr (std::same_as<T, std::vector<YAMLData>>) {
            std::vector<YAMLData> result;
            for (const auto& elem : node) {
                result.emplace_back(elem);
            }
            return result;
        } else if constexpr (!std::same_as<T, YAMLData>) {
            return node.as<T>();
        } else {
            return YAMLData(node.as<yaml>());
        }
    }

    template <typename T>
    void get_value(std::string& p_key, T& p_value) {
        if constexpr (BufferBaseConcept<T>) {
            using BufferT = std::remove_cvref_t<T>;
            std::string result = get_value<std::string>(p_key);
            if (result.size() > BufferT::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format(
                    "Failed to get string value: {}. Buffer capacity: {}. String length: {} (without NUL terminator).",
                    result, BufferT::BUFFER_SIZE, result.size()));
            }
            strncpy(p_value.buffer, result.data(), BufferT::BUFFER_SIZE);
        }
        else {
            p_value = get_value<T>(p_key);
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
    T get_value() const {
        return data.get_value<T>();
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
