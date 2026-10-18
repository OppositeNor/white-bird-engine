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
#include "core/script/sb_lua.hh"
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace WhiteBirdEngine {

namespace {

std::string read_lua_type_stubs() {
    std::ifstream file(WBE_LUA_TYPE_STUBS_PATH);
    EXPECT_TRUE(file.is_open()) << "Missing generated Lua type stubs: " << WBE_LUA_TYPE_STUBS_PATH;
    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

} // namespace

TEST(WBELuaTypeStubsTest, StubsAreValidLua) {
    const std::string stubs = read_lua_type_stubs();
    ASSERT_FALSE(stubs.empty());
    EXPECT_TRUE(stubs.starts_with("---@meta"));
    SBLua lua;
    EXPECT_NO_THROW(lua.load_module("wbe_types", stubs));
}

TEST(WBELuaTypeStubsTest, DeclaresReflectedEnumsClassesAndTasks) {
    const std::string stubs = read_lua_type_stubs();
    EXPECT_NE(stubs.find("---@alias wbe.MeshPartVK\n---| \"OPAQUE\"\n---| \"opaque\"\n"), std::string::npos);
    EXPECT_NE(stubs.find("---@class wbe.RTParamsGraphicsDrawMeshVK\n"), std::string::npos);
    EXPECT_NE(stubs.find("---@field parts wbe.MeshPartVK[]\n"), std::string::npos);
    EXPECT_NE(stubs.find("---@field transform? string\n"), std::string::npos);
    EXPECT_NE(stubs.find("---@class wbe.task.RTGraphicsDrawMeshVK : wbe.RendRecipeTaskVK\n"
                         "---@field type \"RTGraphicsDrawMeshVK\"|\"graphics_draw_mesh\"\n"
                         "---@field params? wbe.RTParamsGraphicsDrawMeshVK\n"),
        std::string::npos);
    EXPECT_NE(stubs.find("---@field tasks? wbe.RecipeTask[]\n"), std::string::npos);
    EXPECT_NE(stubs.find("---@class wbe.RecipeContext\n"), std::string::npos);
    // Test-only reflected types stay out of the editor stubs.
    EXPECT_EQ(stubs.find("TestEnumSigned"), std::string::npos);
}

} // namespace WhiteBirdEngine
