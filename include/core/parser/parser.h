#ifndef __WBE_PARSER_H__
#define __WBE_PARSER_H__

#include "platform/file_system/path.h"
#include <string>
#include <type_traits>
namespace WhiteBirdEngine {

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
     * @brief Get the value of a given key.
     *
     * @tparam T The type of the value.
     * @param p_key The key to get value from.
     * @return The value of the key.
     */
    template <typename T>
    T get_value(const std::string& p_key) {
        return static_cast<ChildType*>(this)->template get_value<T>(p_key);
    }
};

template <typename T>
constexpr bool parser_trait = std::is_base_of<Parser<T>, T>();

template <typename T>
concept ParserConcept = parser_trait<T>;

}

#endif
