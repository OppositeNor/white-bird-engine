#ifndef __WBE_PARSER_H__
#define __WBE_PARSER_H__

#include "platform/file_system/path.h"
#include <string>
#include <type_traits>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class
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
        return static_cast<ChildType*>(this)->template get_value<T>(p_key);
    }

    /**
     * @brief Get all the key values.
     */
    std::vector<std::string> get_all_keys() const {
        return static_cast<ChildType*>(this)->get_all_keys();
    }
};

template <typename T>
constexpr bool is_parser_data = std::is_base_of<ParserData<std::remove_cvref_t<T>>, std::remove_cvref_t<T>>();

template <typename T>
concept ParserDataConcept = is_parser_data<T>;

/**
 * @class
 * @brief Parser class.
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
        return static_cast<ChildType*>(this)->template get_value<T>(p_key);
    }

    /**
     * @brief Get all the key values.
     */
    std::vector<std::string> get_all_keys() const {
        return static_cast<ChildType*>(this)->get_all_keys();
    }
};

template <typename T>
constexpr bool is_parser = std::is_base_of_v<Parser<std::remove_cvref_t<T>>, std::remove_cvref_t<T>>;

template <typename T>
concept ParserConcept = is_parser<T>;

}

#endif
