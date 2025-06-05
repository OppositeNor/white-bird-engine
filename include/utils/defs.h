#ifndef __WBE_DEFS_H__
#define __WBE_DEFS_H__

#include <cstdint>
#ifdef _DEBUG
#include <cassert>
#define WBE_DEBUG_ASSERT(...) assert(__VA_ARGS__)
#else
#define WBE_DEBUG_ASSERT(...)
#endif

#if __GNUC__
#define WBE_NO_OPTIMIZE __attribute__((optimize("O0")))
#else
#warning Please indicate the "no optimization" attribute for your compiler.
#endif

// used for meta-parser
#define WBE_COMPONENT(ConceptName)

#define WBE_TRAIT(TraitName) using TraitType = TraitName

#define WBE_TRAIT_REQURIES(Concept) static_assert(Concept<TraitType>)

namespace WhiteBirdEngine {

constexpr uint64_t MAX_COMPONENT_COUNT = 64;

}

#endif
