#ifndef __WBE_UTILS_H__
#define __WBE_UTILS_H__

#include <cstddef>
#include <cstdint>

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

/**
 * @brief Hash a string in compile time.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
constexpr HashCode static_hash(const char* p_str) {
    // from https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * static_hash(p_str + 1) : 5381;
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


struct EngineConfigOptions {
    // TODO
};


}
#endif

