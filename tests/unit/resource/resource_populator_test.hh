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
#ifndef __WBE_RESOURCE_POPULATOR_TEST_HH__
#define __WBE_RESOURCE_POPULATOR_TEST_HH__

#include "global/global.hh"

#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

class ResourcePopulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
};

TEST_F(ResourcePopulatorTest, TextResourceGeneral) {
    // TODO
}

TEST_F(ResourcePopulatorTest, BinaryReousrceGeneral) {
    // TODO
}

#endif
