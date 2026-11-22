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

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class ShaderVariantResolver
 * @brief Finds the variants of a shader by name. Stateless; copyable and movable.
 *
 * Variant tables are sorted by unique name and searched by binary search. An empty name selects the default variant.
 */
class ShaderVariantResolver final {
public:
    /**
     * @brief Find the variant a name selects in a variant table.
     *
     * @tparam TVariant The entry type.
     * @tparam TGetName Callable returning an entry's name as a std::string_view.
     * @param p_variants The table, sorted by name.
     * @param p_default_index The index of the default variant in the table.
     * @param p_name The variant's name; empty selects the default variant.
     * @param p_get_name Gets an entry's name.
     * @param p_context Description of the table, for the error message.
     * @return The selected entry.
     * @throws std::runtime_error When the table has no variant of the name.
     */
    template <typename TVariant, typename TGetName>
    const TVariant& find(std::span<const TVariant> p_variants, size_t p_default_index, std::string_view p_name,
        TGetName p_get_name, std::string_view p_context) const {
        if (p_name.empty()) {
            return p_variants[p_default_index];
        }
        auto found = std::ranges::lower_bound(p_variants, p_name, {}, p_get_name);
        if (found == p_variants.end() || p_get_name(*found) != p_name) {
            throw std::runtime_error(std::format("Failed to find shader variant of {}: no variant is named '{}'.", p_context, p_name));
        }
        return *found;
    }
};

} // namespace WhiteBirdEngine

#endif
