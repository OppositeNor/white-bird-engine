#ifndef __WBE_UTILS_H__
#define __WBE_UTILS_H__

#include <cstddef>

inline size_t get_align_size(size_t p_size, size_t p_alignment) {
    return ((p_size + p_alignment - 1) / p_alignment) * p_alignment;
}

#endif

