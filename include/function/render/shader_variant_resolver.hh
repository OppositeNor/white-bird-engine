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
#ifndef WBE_FILE_SHADER_VARIANT_RESOLVER_HH
#define WBE_FILE_SHADER_VARIANT_RESOLVER_HH

#include "function/render/shader_macro_value.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class ShaderMacroDeclaration
 * @brief A macro a shader resource enumerates variants over. Copyable and movable.
 *
 * Every variant defines the macro as one of its options, except that an optional macro is also left undefined.
 */
struct ShaderMacroDeclaration {
    /**
     * @brief The macro's name.
     */
    std::string macro;
    /**
     * @brief The values the macro takes, unique and non-empty.
     */
    std::vector<std::string> options;
    /**
     * @brief Whether variants also leave the macro undefined.
     */
    bool optional = false;
};

/**
 * @class ShaderVariantResolver
 * @brief Resolves macro selections to the keys of a shader resource's variants. Stateless; copyable and movable.
 *
 * A variant's key chains the hashes of the macros it defines, sorted by name:
 * hash(hash(hash(hash(hash(<macro 1>), "="), <value 1>), ","), <macro 2>)..., and dynam_hash("") for the variant defining
 * none.
 * Variant tables are sorted by key, with unique keys, and searched by binary search.
 */
class ShaderVariantResolver final {
public:
    /**
     * @brief Compute the key of the variant defining exactly the given macros.
     *
     * @param p_macros The defined macros, in any order, each named once.
     * @return The variant key.
     */
    HashCode make_key(std::vector<ShaderMacroValue> p_macros) const;

    /**
     * @brief Resolve a selection to the key of the variant it names.
     *
     * Each selected macro must be declared, selected once, and take one of its options. A declared macro the selection
     * leaves out is undefined when it is optional, takes its option when it has exactly one, and fails otherwise, so an
     * empty selection names the variant defining no macros whenever every declared macro is optional.
     *
     * @param p_declarations The shader's macro declarations.
     * @param p_selection The selected macro values.
     * @return The key of the selected variant.
     * @throws std::runtime_error When the selection names an undeclared macro, names a macro twice, gives a value outside
     * its options, or leaves out a macro that is neither optional nor single-option.
     */
    HashCode resolve_key(std::span<const ShaderMacroDeclaration> p_declarations,
        const std::vector<ShaderMacroValue>& p_selection) const;

    /**
     * @brief Find the entry of a key in a variant table.
     *
     * @tparam TVariant An entry type with a HashCode `key` member.
     * @param p_variants The table, sorted by key.
     * @param p_key The key to find.
     * @param p_context Description of the table, for the error message.
     * @return The entry of the key.
     * @throws std::runtime_error When the table has no entry of the key.
     */
    template <typename TVariant>
    const TVariant& find(std::span<const TVariant> p_variants, HashCode p_key, std::string_view p_context) const {
        auto found = std::ranges::lower_bound(p_variants, p_key, {}, &TVariant::key);
        if (found == p_variants.end() || found->key != p_key) {
            throw std::runtime_error(std::format("Failed to find shader variant of {}: no variant has key {}.", p_context, p_key));
        }
        return *found;
    }
};

} // namespace WhiteBirdEngine

#endif
