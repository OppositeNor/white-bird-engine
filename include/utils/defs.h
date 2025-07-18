#ifndef __WBE_DEFS_H__
#define __WBE_DEFS_H__

#ifdef _DEBUG
#include <cassert>
// Assersion used in debug builds.
#define WBE_DEBUG_ASSERT(...) assert(__VA_ARGS__)
// Should enable profiling.
#define WBE_ENABLE_PROFILING 1
#else
// Assersion used in debug builds.
#define WBE_DEBUG_ASSERT(...)
// Should enable profiling.
#define WBE_ENABLE_PROFILING 0
#endif

#if __GNUC__
// TODO: fix this
#define WBE_NO_OPTIMIZE __attribute__((optimize("O0")))
#else
#warning Please indicate the "no optimization" attribute here for your compiler.
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
// Platform is little endian.
#define WBE_PLATFORM_LITTLE_ENDIAN

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
// Platform is big endian.
#define WBE_PLATFORM_BIG_ENDIAN

#endif

// Used inside a trait structure.
#define WBE_TRAIT(trait_name) using TraitType = trait_name

// Used inside a trait structure. Requires the trait to satisfy a specific concept.
#define WBE_TRAIT_REQURIES(requires_concept) static_assert(requires_concept<TraitType>)

// Used for macro expansion.
#define WBE_EXPAND_STR(x) #x

// Define a type with metadata.
#define WBE_TYPE_METADATA(type_name) template <> struct TypeIDTrait<type_name> {\
    static constexpr TypeID TYPE_ID = WhiteBirdEngine::static_hash(WBE_EXPAND_STR(type_name));\
    static constexpr std::string_view TYPE_NAME = WBE_EXPAND_STR(type_name);\
};\
template <> struct TypeIDRevTrait<TypeIDTrait<type_name>::TYPE_ID> {\
    using Type = type_name;\
};

// Define a label.
#define WBE_DEFINE_LABEL(label_name) constexpr HashCode label_name = static_hash(WBE_EXPAND_STR(label_name));\
template <> struct LabelTrait<label_name> {\
    static constexpr std::string_view NAME = WBE_EXPAND_STR(label_name);\
};

// Get the label of a label name.
#define WBE_LABEL(label_name) WhiteBirdEngine::static_hash(WBE_EXPAND_STR(label_name))
// Get the label name of a label.
#define WBE_LABEL_NAME(label) WhiteBirdEngine::LabelTrait<label>::NAME

// Get the type from an ID.
#define WBE_TYPE_FROM_ID(type_id) WhiteBirdEngine::TypeIDRevTrait<type_id>::Type
// Get the ID of a type.
#define WBE_TYPE_TO_ID(type_name) WhiteBirdEngine::TypeIDTrait<type_name>::TYPE_ID
// Get the name of the type ID.
#define WBE_TYPE_ID_NAME(type_id) WhiteBirdEngine::TypeIDTrait<WhiteBirdEngine::TypeIDRevTrait<type_id>::Type>::TYPE_NAME

// Kibibyte
#define WBE_KiB(x) x * 1024
// Mebibyte
#define WBE_MiB(x) x * WBE_KiB(1024)
// Gibibyte
#define WBE_GiB(x) x * WBE_MiB(1024)
// Tebibyte
#define WBE_TiB(x) x * WBE_GiB(1024)

// Declare a CRTP concept for a CRTP base class.
#define WBE_DECL_CRTP_CONCEPT(Base) template <typename T>\
concept Base##Concept = std::is_base_of_v<Base<std::remove_cvref_t<T>>, std::remove_cvref_t<T>>

// Labels an component. Used for reflection.
#define WBE_COMPONENT(...)

#endif
