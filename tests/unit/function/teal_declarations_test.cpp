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
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

std::string read_declarations(std::string_view p_file_name) {
    const std::string path = std::string(WBE_TEAL_DECLARATIONS_DIR) + "/" + std::string(p_file_name);
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << "Missing generated Teal declarations: " << path;
    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

} // namespace

TEST(WBETealDeclarationsTest, DeclaresOnlyScriptTypesOfTheEngine) {
    const std::string declarations = read_declarations("wbe.d.tl");
    EXPECT_NE(declarations.find("local record wbe\n"), std::string::npos);
    EXPECT_NE(declarations.find("    enum MeshPartVK\n        \"OPAQUE\"\n        \"opaque\"\n"), std::string::npos);
    // Every script class is an interface; enum fields also accept the integer value.
    EXPECT_NE(declarations.find("    interface RTParamsGraphicsDrawMeshVK\n"), std::string::npos);
    EXPECT_NE(declarations.find("        parts: {MeshPartVK | integer}\n"), std::string::npos);
    EXPECT_NE(declarations.find("        [\"type\"]: string\n"), std::string::npos);
    EXPECT_NE(declarations.find("    interface RendRecipeContextVK\n"), std::string::npos);
    // Reflected types that are not WBE_REFLECT_SCRIPT stay out.
    EXPECT_EQ(declarations.find("RUSchemaAudio"), std::string::npos);
    EXPECT_EQ(declarations.find("EncodeType"), std::string::npos);
    EXPECT_EQ(declarations.find("TestEnumSigned"), std::string::npos);
    EXPECT_NE(declarations.find("\nreturn wbe"), std::string::npos);
}

TEST(WBETealDeclarationsTest, DeclaresTheScriptTypesOfTheTestExecutablesInTestTargets) {
    // The unit tests build with the tests, whose headers reflection scans only then.
    const std::string declarations = read_declarations("wbe.d.tl");
    // A derived script class is an interface that is its bases.
    EXPECT_NE(declarations.find("    interface DeferredRecipeContext is RendRecipeContextVK\n"), std::string::npos);
    EXPECT_NE(declarations.find("    interface SVORecipeParams is RecipeFeatureParams\n"), std::string::npos);
    EXPECT_NE(declarations.find("        static_build: RendRecipeTriggerVK\n"), std::string::npos);
    EXPECT_NE(declarations.find("        variant: SVORecipeVariant | integer\n"), std::string::npos);
}

} // namespace WhiteBirdEngine
