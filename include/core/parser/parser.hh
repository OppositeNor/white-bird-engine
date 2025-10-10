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
#ifndef __WBE_PARSER_HH__
#define __WBE_PARSER_HH__

#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include <string>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class ParserData
 * @brief Data for the parser.
 *
 * @tparam ChildType The type of the child class for CRTP
 */
template <typename ChildType>
class ParserData {
public:
    /**
     * @brief Get the value of the parser data from a key.
     *
     * @tparam T The type of the value to get.
     * @param p_key The key of the value.
     * @return The value of the key.
     */
    template <typename T>
    T get_value(const std::string& p_key) const {
        return static_cast<const ChildType*>(this)->template get_value<T>(p_key);
    }

    /**
     * @brief Get the value of the parser data.
     *
     * @tparam T The type of the value to get.
     * @return The value of the parser data.
     */
    template <typename T>
    T get_value() const {
        return static_cast<ChildType*>(this)->template get_value<T>();
    }

    /**
     * @brief Get all the key values.
     */
    std::vector<std::string> get_all_keys() const {
        return static_cast<const ChildType*>(this)->get_all_keys();
    }

    /**
     * @brief Check if contains a key.
     *
     * @return True if contains a key, false otherwise.
     */
    bool contains(const std::string& p_key) const {
        return static_cast<const ChildType*>(this)->contains(p_key);
    }
};

WBE_DECL_CRTP_CONCEPT(ParserData);

template <ParserDataConcept ParserDataType>
inline std::ostream& operator<<(std::ostream& p_ostream, const ParserData<ParserDataType>& p_parser) {
    return operator<<(p_ostream, p_parser);
}


/**
 * @class Parser
 * @brief Parser interface.
 *
 * @tparam ChildType The type of the child class for CRTP
 */
template <typename ChildType>
class Parser {
public:
    Parser() = default;
    ~Parser() {}

    /**
     * @brief Parse a file from a path.
     *
     * @param p_path The path to parse file from.
     */
    void parse(const Path& p_path) {
        return static_cast<ChildType*>(this)->parse(p_path);
    }

    /**
     * @brief Parse a file from buffer.
     *
     * @param p_buffer The buffer to parse from.
     */
    void parse_from_buffer(const std::string& p_buffer) {
        return static_cast<ChildType*>(this)->parse_from_buffer(p_buffer);
    }

    /**
     * @brief Get the value of a given key.
     *
     * @tparam T The type of the value.
     * @param p_key The key to get value from.
     * @return The value of the key.
     */
    template <typename T>
    T get_value(const std::string& p_key) const {
        return static_cast<const ChildType*>(this)->template get_value<T>(p_key);
    }

    /**
     * @brief Get all the key values.
     */
    std::vector<std::string> get_all_keys() const {
        return static_cast<const ChildType*>(this)->get_all_keys();
    }

    /**
     * @brief Get the data of the parser.
     *
     * @return The data of the paraser.
     */
    const auto& get_data() const {
        return static_cast<const ChildType*>(this)->get_data();
    }

    /**
     * @brief Get the data of the parser.
     *
     * @return The data of the parser.
     */
    auto& get_data() {
        return static_cast<ChildType*>(this)->get_data();
    }

    /**
     * @brief Check if contains a key.
     *
     * @return True if contains a key, false otherwise.
     */
    bool contains(const std::string& p_key) const {
        return static_cast<const ChildType*>(this)->contains();
    }

    /**
     * @brief Get the value and set.
     *
     * @tparam T The type of the value to get.
     * @param p_key The key of the value.
     * @param p_value The value reference to be set.
     */
    template <typename T>
    void get_value(std::string& p_key, T&& p_value) const {
        return static_cast<const ChildType*>(this)->get_value(p_key, std::forward<T>(p_value));
    }
};

WBE_DECL_CRTP_CONCEPT(Parser);

}

#endif
