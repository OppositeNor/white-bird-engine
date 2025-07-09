#ifndef __WBE_DEFS_H__
#define __WBE_DEFS_H__

#ifdef _DEBUG
#include <cassert>
#define WBE_DEBUG_ASSERT(...) assert(__VA_ARGS__)
#define WBE_ENABLE_PROFILING 1
#else
#define WBE_DEBUG_ASSERT(...)
#define WBE_ENABLE_PROFILING 0
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

#define WBE_TRAIT(trait_name) using TraitType = trait_name

#define WBE_TRAIT_REQURIES(requires_concept) static_assert(requires_concept<TraitType>)

#define WBE_EXPAND_STR(x) #x

#define WBE_TYPE_METADATA(type_name) template <> struct TypeIDTrait<type_name> {\
    static constexpr TypeID TYPE_ID = WhiteBirdEngine::static_hash(WBE_EXPAND_STR(type_name));\
    static constexpr std::string_view TYPE_NAME = WBE_EXPAND_STR(type_name);\
};\
template <> struct TypeIDRevTrait<TypeIDTrait<type_name>::TYPE_ID> {\
    using Type = type_name;\
};

#define WBE_DEFINE_LABEL(label_name) constexpr HashCode label_name = static_hash(WBE_EXPAND_STR(label_name));\
template <> struct LabelTrait<label_name> {\
    static constexpr std::string_view NAME = WBE_EXPAND_STR(label_name);\
};

#define WBE_LABEL(label_name) WhiteBirdEngine::static_hash(WBE_EXPAND_STR(label_name))
#define WBE_LABEL_NAME(label) WhiteBirdEngine::LabelTrait<label>::NAME

#define WBE_TYPE_FROM_ID(type_id) WhiteBirdEngine::TypeIDRevTrait<type_id>::Type
#define WBE_TYPE_TO_ID(type_name) WhiteBirdEngine::TypeIDTrait<type_name>::TYPE_ID
#define WBE_TYPE_ID_NAME(type_id) WhiteBirdEngine::TypeIDTrait<WhiteBirdEngine::TypeIDRevTrait<type_id>::Type>::TYPE_NAME

#define WBE_KiB(x) x * 1024
#define WBE_MiB(x) x * WBE_KiB(1024)
#define WBE_GiB(x) x * WBE_MiB(1024)
#define WBE_TiB(x) x * WBE_GiB(1024)

#define WBE_DECL_CRTP_CONCEPT(Base) template <typename T>\
concept Base##Concept = std::is_base_of_v<Base<std::remove_cvref_t<T>>, std::remove_cvref_t<T>>

#endif
