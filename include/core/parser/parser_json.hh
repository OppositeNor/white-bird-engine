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
#ifndef __WBE_PARSER_JSON_HH__
#define __WBE_PARSER_JSON_HH__

#include "parser.hh"
#include "utils/utils.hh"
#include <concepts>
#include <cstring>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace WhiteBirdEngine {

/**
 * @class JSONData
 * @brief Data class for json.
 *
 */
class JSONData final : public ParserData<JSONData> {
    using json = nlohmann::json;
    friend class ParserJSON;
    friend std::ostream& operator<<(std::ostream& p_ostrem, const JSONData& p_parser);
public:
    JSONData() {}
    ~JSONData() {}
    JSONData(const JSONData& p_other) : data(p_other.data) {}
    JSONData(JSONData&& p_other) : data(std::move(p_other.data)) {}
    JSONData& operator=(const JSONData& p_other) {
        data = p_other.data;
        return *this;
    }
    JSONData& operator=(JSONData&& p_other) {
        data = std::move(p_other.data);
        return *this;
    }

    JSONData(const json& p_data)
        : data(p_data) {}
    JSONData(json&& p_data)
        : data(std::move(p_data)) {}

    template <typename T>
    T get_value(const std::string& p_key) const {
        if constexpr (std::same_as<T, std::vector<JSONData>>) {
            std::vector<JSONData> result;
            for (const auto& elem : data.at(p_key)) {
                result.emplace_back(elem);
            }
            return result;
        } else if constexpr (!std::same_as<T, JSONData>) {
            return data.at(p_key).get<T>();
        } else {
            return JSONData(data.at(p_key).get<json>());
        }
    }

    template <typename T>
    T get_value() const {
        if constexpr (std::same_as<T, std::vector<JSONData>>) {
            std::vector<JSONData> result;
            for (const auto& elem : data) {
                result.emplace_back(elem);
            }
            return result;
        } else if constexpr (!std::same_as<T, JSONData>) {
            return data.get<T>();
        } else {
            return JSONData(data.get<json>());
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

    /**
     * @brief Get all keys.
     * @todo Test
     *
     * @return All the keys.
     */
    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> result;
        result.reserve(data.size());
        for (json::const_iterator it = data.begin(); it != data.end(); ++it) {
            result.push_back(it.key());
        }
        return result;
    }

    bool contains(const std::string& p_key) const {
        return data.contains(p_key);
    }

private:
    json data;
};

inline std::ostream& operator<<(std::ostream& p_ostream, const JSONData& p_parser) {
    return p_ostream << p_parser.data;
}

/**
 * @class ParserJSON
 * @brief Parser for JSON files.
 * @todo Test
 */
class ParserJSON final : public Parser<ParserJSON> {
    using json = nlohmann::json;
public:
    using DataType = JSONData;

    ParserJSON() = default;
    virtual ~ParserJSON() {}
    ParserJSON(const ParserJSON& p_other)
        : data(p_other.data) {}
    ParserJSON(ParserJSON&& p_other)
        : data(std::move(p_other.data)) {}
    ParserJSON& operator=(const ParserJSON& p_other) {
        data = p_other.data;
        return *this;
    }
    ParserJSON& operator=(ParserJSON&& p_other) {
        data = std::move(p_other.data);
        return *this;
    }

    void parse(const Path& p_path) {
        std::ifstream f;
        f.open(static_cast<std::string>(p_path));
        if (!f.is_open()) {
            throw std::runtime_error("Failed to open file at path: " + static_cast<std::string>(p_path));
        }
        data.data = json::parse(f);
    }

    void parse_from_buffer(const std::string& p_buffer) {
        data.data = json::parse(p_buffer);
    }

    template <typename T>
    T get_value(const std::string& p_key) const {
        return data.get_value<T>(p_key);
    }

    template <typename T>
    T get_value() const {
        return data.get_value<T>();
    }

    template <typename T>
    void get_value(std::string& p_key, T&& p_value) {
        return data.get_value<T>(std::forward<T>(p_value));
    }

    /**
     * @brief Get all keys.
     * @todo Test
     *
     * @return All the keys.
     */
    std::vector<std::string> get_all_keys() const {
        return data.get_all_keys();
    }

    const JSONData& get_data() const {
        return data;
    }

    JSONData& get_data() {
        return data;
    }

    bool contains(const std::string& p_key) const {
        return data.contains(p_key);
    }

private:
    JSONData data;
};

}

#endif
