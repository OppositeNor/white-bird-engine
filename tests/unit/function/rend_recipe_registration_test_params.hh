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
#ifndef WBE_FILE_REND_RECIPE_REGISTRATION_TEST_PARAMS_HH
#define WBE_FILE_REND_RECIPE_REGISTRATION_TEST_PARAMS_HH

#include "core/reflection/reflection_defs.hh"
#include <string>

namespace WhiteBirdEngine {

/**
 * @class RendRecipeRegistrationTestParams
 * @brief A reflected description of the recipe-builder registration tests, so recipes can name its type.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION) RendRecipeRegistrationTestParams {
    /**
     * @brief An arbitrary value; registrations with different values compile again.
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string value;

    bool operator==(const RendRecipeRegistrationTestParams&) const = default;
};

} // namespace WhiteBirdEngine

#endif
