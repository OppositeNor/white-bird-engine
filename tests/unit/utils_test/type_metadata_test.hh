/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_TYPE_METADATA_TEST_HH__
#define __WBE_TYPE_METADATA_TEST_HH__
#include "utils/defs.hh"
#include "utils/utils.hh"
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
