#ifndef __WBE_UTILS_H__
#define __WBE_UTILS_H__

#include <cstddef>

/**
 * @brief Get the size after alignment.
 *
 * @param p_size Size.
 * @param p_alignment Alignment.
 * @return The size after alignment.
 */
inline size_t get_align_size(size_t p_size, size_t p_alignment) {
    if (p_alignment == 0) {
        return p_size;
    }
    return ((p_size + p_alignment - 1) / p_alignment) * p_alignment;
}

#endif

