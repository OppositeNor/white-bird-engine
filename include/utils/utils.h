#ifndef __WBE_UTILS_H__
#define __WBE_UTILS_H__

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @brief Get the size after alignment.
 *
 * @param p_size Size.
 * @param p_alignment Alignment. If alignment is 0, no alignment will be made.
 * @return The size after alignment.
 */
constexpr size_t get_align_size(size_t p_size, size_t p_alignment) {
    return p_alignment == 0 ? p_size : ((p_size + p_alignment - 1) / p_alignment) * p_alignment;
}

using HashCode = uint32_t;
using UUID = uint32_t;

/**
 * @brief Hash a string in compile time.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
consteval HashCode static_hash(const char* p_str) {
    // from https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * static_hash(p_str + 1) : 5381;
}

/**
 * @brief Hash a string.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
constexpr HashCode dynam_hash(const char* p_str) {
    // from https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * dynam_hash(p_str + 1) : 5381;
}

using TypeID = HashCode;
using ConstID = HashCode;

template <typename T>
struct TypeIDTrait;

template <TypeID T>
struct TypeIDRevTrait;

template <HashCode CODE>
struct LabelTrait;

constexpr uint64_t MAX_COMPONENT_COUNT = 64;

/**
 * @brief Split the string with respect to a token.
 *
 * @param p_str The string to split.
 * @param p_token The token.
 * @return Array of strings.
 */
inline std::vector<std::string> split_string(std::string p_str, char p_token) {
    std::vector<std::string> result;
    auto split_pos = p_str.find(p_token);
    while (split_pos != std::string::npos) {
        result.push_back(p_str.substr(0, split_pos));
        p_str.erase(0, split_pos + 1);
        split_pos = p_str.find(p_token);
    }
    return result;
}

/**
 * @brief Get the position that any of the tokens is first found in the string.
 *
 * @param p_str The string to find the token.
 * @param p_tokens The tokens to look for.
 * @return The index of the token that is first found.
 */
inline size_t find_first_pos(const std::string& p_str, const std::string& p_tokens) {
    if (p_tokens.size() == 0) {
        return std::string::npos;
    }
    size_t min_pos = std::numeric_limits<size_t>::infinity();
    for (char c : p_tokens) {
        min_pos = std::min(min_pos, p_str.find(c));
    }
    return min_pos;
}

/**
 * @brief Split the string with respect to a list of tokens.
 *
 * @todo Test
 * @param p_str The string to split.
 * @param p_token The list of tokens.
 * @return Array of strings.
 */
inline std::vector<std::string> split_string(std::string p_str, const std::string& p_tokens) {
    std::vector<std::string> result;
    auto split_pos = find_first_pos(p_str, p_tokens);
    while (split_pos != std::string::npos) {
        result.push_back(p_str.substr(0, split_pos));
        p_str.erase(0, split_pos);
        split_pos = find_first_pos(p_str, p_tokens);
    }
    return result;
}

struct EngineConfigOptions {
    size_t single_tick_stack_size = 1024;
    size_t global_pool_size = 2048;
    size_t thread_pool_size = 2048;
};

enum ColorSpace {
    RGB,
    SRGB
};

}
#endif

