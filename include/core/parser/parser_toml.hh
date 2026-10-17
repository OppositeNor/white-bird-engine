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
#ifndef WBE_FILE_PARSER_TOML_HH
#define WBE_FILE_PARSER_TOML_HH

#include "glm/detail/qualifier.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/fwd.hpp"
#include "parser.hh"
#include "platform/file_system/path.hh"
#include "utils/utils.hh"
#include <concepts>
#include <cstdint>
#include <cstring>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <type_traits>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @brief Satisfied by std::vector specializations with the default allocator.
 */
template <typename T>
concept TOMLStdVectorConcept = requires { typename T::value_type; } && std::same_as<T, std::vector<typename T::value_type>>;

/**
 * @class TOMLData
 * @brief Data class for TOML.
 *
 * Holds one TOML node of any kind (table, array, or value), or nothing. A default-constructed TOMLData holds nothing; it
 * reports no keys and is written as an empty table when stored inside another TOMLData, because TOML has no null.
 * Copying performs a deep copy. Moving leaves the moved-from object empty.
 *
 * Integers are stored as signed 64-bit TOML integers; storing an integer outside that range throws. Floating-point reads
 * also accept TOML integers. Every failed lookup or type mismatch throws std::runtime_error.
 */
class TOMLData final : public ParserData<TOMLData> {
    friend class ParserTOML;
    friend std::ostream& operator<<(std::ostream& p_ostream, const TOMLData& p_parser);

public:
    TOMLData() = default;
    ~TOMLData() = default;
    TOMLData(const TOMLData& p_other) : holder(p_other.holder) {
    }
    TOMLData(TOMLData&& p_other) noexcept : holder(std::move(p_other.holder)) {
    }
    TOMLData& operator=(const TOMLData& p_other) {
        if (&p_other == this) {
            return *this;
        }
        holder = p_other.holder;
        return *this;
    }
    TOMLData& operator=(TOMLData&& p_other) noexcept {
        if (&p_other == this) {
            return *this;
        }
        holder = std::move(p_other.holder);
        return *this;
    }

    /**
     * @brief Construct from a copy of a TOML node.
     *
     * @param p_node The node to copy.
     */
    explicit TOMLData(const toml::node& p_node) {
        holder.push_back(p_node);
    }

    /**
     * @brief Construct by taking a TOML table.
     *
     * @param p_table The table to take.
     */
    explicit TOMLData(toml::table&& p_table) {
        holder.push_back(std::move(p_table));
    }

    /**
     * @brief Set a value of a key. Replaces the held node with an empty table first when it is not a table.
     *
     * @tparam T The type of the value to set.
     * @param p_key The key of the value.
     * @param p_value The value to set to.
     */
    template <typename T>
    void set_value(std::string_view p_key, T&& p_value) {
        toml::array value_holder;
        append_value(value_holder, std::forward<T>(p_value));
        if (holder.empty() || !holder[0].is_table()) {
            holder.clear();
            holder.push_back(toml::table{});
        }
        holder[0].as_table()->insert_or_assign(p_key, std::move(value_holder[0]));
    }

    /**
     * @brief Set the current parser data to a value.
     *
     * @tparam T The type of the value to set.
     * @param p_value The value to set to.
     */
    template <typename T>
    void set(T&& p_value) {
        toml::array value_holder;
        append_value(value_holder, std::forward<T>(p_value));
        holder = std::move(value_holder);
    }

    template <typename T>
    T get_value(std::string_view p_key) const {
        T result;
        get_value<T>(p_key, result);
        return result;
    }

    template <typename T>
    T get() const {
        T result;
        get<T>(result);
        return result;
    }

    template <typename T>
    void get(T& p_value) const {
        if (holder.empty()) {
            throw std::runtime_error("Failed to get TOML value: the data is empty.");
        }
        read_node(holder[0], p_value);
    }

    template <typename T>
    void get_value(std::string_view p_key, T& p_value) const {
        read_node(get_child(p_key), p_value);
    }

    /**
     * @brief Get all keys of the held table.
     *
     * @return All the keys. Empty when the data is empty.
     */
    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> result;
        if (holder.empty()) {
            return result;
        }
        const toml::table* table = holder[0].as_table();
        if (table == nullptr) {
            throw std::runtime_error("Failed to get TOML keys: the data is not a table.");
        }
        result.reserve(table->size());
        for (const auto& [key, value] : *table) {
            result.emplace_back(key.str());
        }
        return result;
    }

    bool contains(std::string_view p_key) const {
        if (holder.empty()) {
            return false;
        }
        const toml::table* table = holder[0].as_table();
        return table != nullptr && table->contains(p_key);
    }

    bool is_string() const {
        return !holder.empty() && holder[0].is_string();
    }

private:
    // Holds zero or one element: the data's node. toml::array owns polymorphic nodes and deep-copies them.
    toml::array holder;

    const toml::node& get_child(std::string_view p_key) const {
        if (holder.empty()) {
            throw std::runtime_error(std::format("Failed to get TOML value of key \"{}\": the data is empty.", p_key));
        }
        const toml::table* table = holder[0].as_table();
        if (table == nullptr) {
            throw std::runtime_error(std::format("Failed to get TOML value of key \"{}\": the data is not a table.", p_key));
        }
        const toml::node* child = table->get(p_key);
        if (child == nullptr) {
            throw std::runtime_error(std::format("Failed to get TOML value: key \"{}\" does not exist.", p_key));
        }
        return *child;
    }

    static const toml::node& get_field(const toml::node& p_node, std::string_view p_key) {
        const toml::table* table = p_node.as_table();
        if (table == nullptr) {
            throw std::runtime_error(std::format("Failed to get TOML field \"{}\": the node is not a table.", p_key));
        }
        const toml::node* child = table->get(p_key);
        if (child == nullptr) {
            throw std::runtime_error(std::format("Failed to get TOML field: key \"{}\" does not exist.", p_key));
        }
        return *child;
    }

    static float read_float_field(const toml::node& p_node, std::string_view p_key) {
        float result = 0.0F;
        read_node(get_field(p_node, p_key), result);
        return result;
    }

    template <typename T>
    static void read_node(const toml::node& p_node, T& p_value) {
        if constexpr (std::same_as<T, TOMLData>) {
            p_value = TOMLData(p_node);
        } else if constexpr (std::same_as<T, std::vector<TOMLData>>) {
            const toml::array* array = p_node.as_array();
            if (array == nullptr) {
                throw std::runtime_error("Failed to get std::vector<TOMLData>: the node is not an array.");
            }
            p_value.clear();
            p_value.reserve(array->size());
            for (const toml::node& element : *array) {
                p_value.emplace_back(element);
            }
        } else if constexpr (BufferBaseConcept<T>) {
            using BufferT = std::remove_cvref_t<T>;
            std::string result;
            read_node(p_node, result);
            if (result.size() > BufferT::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format("Failed to get string value: {}. Buffer capacity: {}. "
                                                     "String length: {} (without NUL terminator).",
                    result,
                    BufferT::BUFFER_SIZE,
                    result.size()));
            }
            strncpy(p_value.buffer, result.data(), BufferT::BUFFER_SIZE - 1);
            p_value.buffer[BufferT::BUFFER_SIZE - 1] = '\0';
        } else if constexpr (std::same_as<T, glm::vec2>) {
            const toml::table* table = p_node.as_table();
            if (table != nullptr && table->contains("u")) {
                // Support for UV notation.
                p_value = glm::vec2(read_float_field(p_node, "u"), read_float_field(p_node, "v"));
            } else {
                p_value = glm::vec2(read_float_field(p_node, "x"), read_float_field(p_node, "y"));
            }
        } else if constexpr (std::same_as<T, glm::vec3>) {
            p_value = glm::vec3(read_float_field(p_node, "x"), read_float_field(p_node, "y"), read_float_field(p_node, "z"));
        } else if constexpr (std::same_as<T, glm::vec4>) {
            p_value = glm::vec4(
                read_float_field(p_node, "x"), read_float_field(p_node, "y"), read_float_field(p_node, "z"), read_float_field(p_node, "w"));
        } else if constexpr (std::same_as<T, glm::quat>) {
            p_value = glm::quat(
                read_float_field(p_node, "w"), read_float_field(p_node, "x"), read_float_field(p_node, "y"), read_float_field(p_node, "z"));
        } else if constexpr (std::same_as<T, glm::mat4>) {
            const toml::array* array = p_node.as_array();
            if (array == nullptr || array->size() != 16) {
                throw std::runtime_error("Failed to get glm::mat4: expected an array containing exactly 16 values.");
            }
            for (glm::length_t column = 0; column < 4; ++column) {
                for (glm::length_t row = 0; row < 4; ++row) {
                    read_node((*array)[static_cast<size_t>((column * 4) + row)], p_value[column][row]);
                }
            }
        } else if constexpr (std::same_as<T, bool>) {
            const toml::value<bool>* value = p_node.as_boolean();
            if (value == nullptr) {
                throw std::runtime_error("Failed to get bool: the TOML node is not a boolean.");
            }
            p_value = value->get();
        } else if constexpr (std::same_as<T, std::string>) {
            const toml::value<std::string>* value = p_node.as_string();
            if (value == nullptr) {
                throw std::runtime_error("Failed to get std::string: the TOML node is not a string.");
            }
            p_value = value->get();
        } else if constexpr (std::integral<T>) {
            const toml::value<int64_t>* value = p_node.as_integer();
            if (value == nullptr) {
                throw std::runtime_error("Failed to get integer: the TOML node is not an integer.");
            }
            if (!std::in_range<T>(value->get())) {
                throw std::runtime_error(std::format("Failed to get integer: {} is out of the target type's range.", value->get()));
            }
            p_value = static_cast<T>(value->get());
        } else if constexpr (std::floating_point<T>) {
            if (const toml::value<double>* value = p_node.as_floating_point()) {
                p_value = static_cast<T>(value->get());
            } else if (const toml::value<int64_t>* integer_value = p_node.as_integer()) {
                p_value = static_cast<T>(integer_value->get());
            } else {
                throw std::runtime_error("Failed to get floating-point value: the TOML node is not a number.");
            }
        } else if constexpr (TOMLStdVectorConcept<T>) {
            const toml::array* array = p_node.as_array();
            if (array == nullptr) {
                throw std::runtime_error("Failed to get std::vector: the TOML node is not an array.");
            }
            p_value.clear();
            p_value.reserve(array->size());
            for (const toml::node& element : *array) {
                typename T::value_type element_value{};
                read_node(element, element_value);
                p_value.push_back(std::move(element_value));
            }
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type for TOMLData.");
        }
    }

    template <typename T>
    static void append_value(toml::array& p_array, T&& p_value) {
        using Type = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Type, TOMLData>) {
            if (p_value.holder.empty()) {
                p_array.push_back(toml::table{});
            } else {
                p_array.push_back(p_value.holder[0]);
            }
        } else if constexpr (BufferBaseConcept<Type>) {
            std::string result = p_value.buffer;
            if (result.size() > Type::BUFFER_SIZE - 1) {
                throw std::runtime_error(std::format("Failed to get string value: {}. Buffer capacity: {}. "
                                                     "String length: {} (without NUL terminator).",
                    result,
                    Type::BUFFER_SIZE,
                    result.size()));
            }
            p_array.push_back(std::move(result));
        } else if constexpr (std::same_as<Type, glm::vec2>) {
            p_array.push_back(toml::table{
                {"x", static_cast<double>(p_value.x)},
                {"y", static_cast<double>(p_value.y)},
            });
        } else if constexpr (std::same_as<Type, glm::vec3>) {
            p_array.push_back(toml::table{
                {"x", static_cast<double>(p_value.x)},
                {"y", static_cast<double>(p_value.y)},
                {"z", static_cast<double>(p_value.z)},
            });
        } else if constexpr (std::same_as<Type, glm::vec4> || std::same_as<Type, glm::quat>) {
            p_array.push_back(toml::table{
                {"x", static_cast<double>(p_value.x)},
                {"y", static_cast<double>(p_value.y)},
                {"z", static_cast<double>(p_value.z)},
                {"w", static_cast<double>(p_value.w)},
            });
        } else if constexpr (std::same_as<Type, glm::mat4>) {
            toml::array matrix;
            for (glm::length_t column = 0; column < 4; ++column) {
                for (glm::length_t row = 0; row < 4; ++row) {
                    matrix.push_back(static_cast<double>(p_value[column][row]));
                }
            }
            p_array.push_back(std::move(matrix));
        } else if constexpr (std::same_as<Type, bool>) {
            p_array.push_back(static_cast<bool>(p_value));
        } else if constexpr (std::convertible_to<const Type&, std::string_view>) {
            p_array.push_back(std::string(std::string_view(p_value)));
        } else if constexpr (std::integral<Type>) {
            if (!std::in_range<int64_t>(p_value)) {
                throw std::runtime_error(std::format("Failed to set TOML integer: {} does not fit in a signed 64-bit integer.", p_value));
            }
            p_array.push_back(static_cast<int64_t>(p_value));
        } else if constexpr (std::floating_point<Type>) {
            p_array.push_back(static_cast<double>(p_value));
        } else if constexpr (TOMLStdVectorConcept<Type>) {
            toml::array result;
            for (const auto& element : p_value) {
                append_value(result, element);
            }
            p_array.push_back(std::move(result));
        } else {
            static_assert(sizeof(Type) == 0, "Unsupported type for TOMLData.");
        }
    }
};

inline std::ostream& operator<<(std::ostream& p_ostream, const TOMLData& p_parser) {
    if (p_parser.holder.empty()) {
        return p_ostream;
    }
    p_parser.holder[0].visit([&p_ostream](const auto& p_node) {
        p_ostream << p_node;
    });
    return p_ostream;
}

/**
 * @class ParserTOML
 * @brief Parser for TOML files.
 *
 * Parse failures throw std::runtime_error with the source location of the error. Copyable and movable; copies are deep.
 */
class ParserTOML final : public Parser<ParserTOML> {
public:
    using DataType = TOMLData;

    ParserTOML() = default;
    virtual ~ParserTOML() = default;
    ParserTOML(const ParserTOML& p_other) : data(p_other.data) {
    }
    ParserTOML(ParserTOML&& p_other) noexcept : data(std::move(p_other.data)) {
    }
    ParserTOML& operator=(const ParserTOML& p_other) {
        if (&p_other == this) {
            return *this;
        }
        data = p_other.data;
        return *this;
    }
    ParserTOML& operator=(ParserTOML&& p_other) noexcept {
        if (&p_other == this) {
            return *this;
        }
        data = std::move(p_other.data);
        return *this;
    }

    void parse(const Path& p_path);

    void parse_from_buffer(std::string_view p_buffer);

    template <typename T>
    T get_value(std::string_view p_key) const {
        return data.get_value<T>(p_key);
    }

    template <typename T>
    void get_value(std::string_view p_key, T& p_val) const {
        data.get_value<T>(p_key, p_val);
    }

    template <typename T>
    T get() const {
        return data.get<T>();
    }

    template <typename T>
    void get(T& p_val) const {
        data.get<T>(p_val);
    }

    std::vector<std::string> get_all_keys() const {
        return data.get_all_keys();
    }

    const TOMLData& get_data() const {
        return data;
    }

    TOMLData& get_data() {
        return data;
    }

    bool contains(std::string_view p_key) const {
        return data.contains(p_key);
    }

private:
    TOMLData data;
};

} // namespace WhiteBirdEngine

#endif
