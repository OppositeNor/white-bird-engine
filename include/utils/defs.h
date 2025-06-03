#ifndef __WBE_DEFS_H__
#define __WBE_DEFS_H__

#include <cstdint>
#ifdef _DEBUG
#include <cassert>
#define WBE_DEBUG_ASSERT(...) assert(__VA_ARGS__)
#else
#define WBE_DEBUG_ASSERT(...)
#endif

namespace WhiteBirdEngine {

constexpr uint64_t MAX_COMPONENT_COUNT = 64;

}

#endif
