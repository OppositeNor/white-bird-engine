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
