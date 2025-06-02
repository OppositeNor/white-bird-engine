#ifndef __PE_DEFS_H__
#define __PE_DEFS_H__

#ifdef _DEBUG
#include <cassert>
#define PE_DEBUG_ASSERT(...) assert(__VA_ARGS__)
#else
#define PE_DEBUG_ASSERT(x)
#endif

#endif
