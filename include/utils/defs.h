#ifndef __WBE_DEFS_H__
#define __WBE_DEFS_H__

#ifdef _DEBUG
#include <cassert>
#define WBE_DEBUG_ASSERT(...) assert(__VA_ARGS__)
#else
#define WBE_DEBUG_ASSERT(...)
#endif

#if __GNUC__
// TODO: fix this
#define WBE_NO_OPTIMIZE __attribute__((optimize("O0")))
#else
#warning Please indicate the "no optimization" attribute here for your compiler.
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define WBE_PLATFORM_LITTLE_ENDIAN

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WBE_PLATFORM_BIG_ENDIAN

#endif


#define WBE_TRAIT(TraitName) using TraitType = TraitName

#define WBE_TRAIT_REQURIES(Concept) static_assert(Concept<TraitType>)

#define WBE_TYPE_METADATA(TypeName) template <> struct TypeIDTrait<TypeName> {\
    static constexpr TypeID TYPE_ID = static_hash(#TypeName);\
};\
template <> struct TypeIDRevTrait<TypeIDTrait<TypeName>::TYPE_ID> {\
    using Type = TypeName;\
};

#define WBE_TYPE_FROM_ID(type_id) WhiteBirdEngine::TypeIDRevTrait<type_id>::Type
#define WBE_TYPE_TO_ID(TypeName) WhiteBirdEngine::TypeIDTrait<TypeName>::TYPE_ID


#endif
