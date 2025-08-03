#ifndef __WBE_TYPE_METADATA_TEST_H__
#define __WBE_TYPE_METADATA_TEST_H__
#include "utils/defs.h"
#include "utils/utils.h"
#include <gtest/gtest.h>

struct TypeA {};

namespace WhiteBirdEngine {
struct TypeB {};
WBE_TYPE_METADATA(TypeA);
WBE_TYPE_METADATA(TypeB);
}

namespace WBE = WhiteBirdEngine;

TEST(WBETypeMetadataTest, Hashing) {
    constexpr uint32_t hash_hello = WBE::static_hash("Hello!");
    constexpr uint32_t hash_hello2 = WBE::static_hash("Hello!");
    constexpr uint32_t hash_goodbye = WBE::static_hash("Goodbye!");
    ASSERT_EQ(hash_hello, hash_hello2);
    ASSERT_NE(hash_hello, hash_goodbye);
}

TEST(WBETypeMetadataTest, Metadata) {
    ASSERT_NE(WBE_TYPE_TO_ID(TypeA), WBE_TYPE_TO_ID(WBE::TypeB));
    ASSERT_TRUE((std::is_same_v<WBE_TYPE_FROM_ID(WBE::TypeIDTrait<TypeA>::TYPE_ID), TypeA>));
    ASSERT_TRUE((std::is_same_v<WBE_TYPE_FROM_ID(WBE::TypeIDTrait<WBE::TypeB>::TYPE_ID), WBE::TypeB>));
    ASSERT_EQ(WBE_TYPE_ID_NAME(WBE_TYPE_TO_ID(TypeA)), std::string("TypeA"));
}

#endif
