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
#include "function/render/shader_variant_resolver.hh"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

namespace {
const ShaderMacroValue* find_selected(const std::vector<ShaderMacroValue>& p_selection, std::string_view p_macro) {
    auto found = std::ranges::find(p_selection, p_macro, &ShaderMacroValue::macro);
    return found == p_selection.end() ? nullptr : &*found;
}
} // namespace

HashCode ShaderVariantResolver::make_key(std::vector<ShaderMacroValue> p_macros) const {
    std::ranges::sort(p_macros, {}, &ShaderMacroValue::macro);
    if (p_macros.empty()) {
        return dynam_hash(std::string_view());
    }
    HashCode key = dynam_hash(std::string_view(p_macros.front().macro));
    key = dynam_hash_append(key, "=");
    key = dynam_hash_append(key, p_macros.front().value);
    for (size_t i = 1; i < p_macros.size(); ++i) {
        key = dynam_hash_append(key, ",");
        key = dynam_hash_append(key, p_macros[i].macro);
        key = dynam_hash_append(key, "=");
        key = dynam_hash_append(key, p_macros[i].value);
    }
    return key;
}

HashCode ShaderVariantResolver::resolve_key(
    std::span<const ShaderMacroDeclaration> p_declarations, const std::vector<ShaderMacroValue>& p_selection) const {
    for (size_t i = 0; i < p_selection.size(); ++i) {
        const ShaderMacroValue& selected = p_selection[i];
        auto declaration = std::ranges::find(p_declarations, selected.macro, &ShaderMacroDeclaration::macro);
        if (declaration == p_declarations.end()) {
            throw std::runtime_error(std::format("Failed to resolve shader variant: macro '{}' is not declared by the shader.", selected.macro));
        }
        for (size_t j = 0; j < i; ++j) {
            if (p_selection[j].macro == selected.macro) {
                throw std::runtime_error(std::format("Failed to resolve shader variant: macro '{}' is selected more than once.", selected.macro));
            }
        }
        if (std::ranges::find(declaration->options, selected.value) == declaration->options.end()) {
            throw std::runtime_error(std::format("Failed to resolve shader variant: '{}' is not an option of macro '{}'.", selected.value, selected.macro));
        }
    }
    std::vector<ShaderMacroValue> defined;
    defined.reserve(p_declarations.size());
    for (const ShaderMacroDeclaration& declaration : p_declarations) {
        const ShaderMacroValue* selected = find_selected(p_selection, declaration.macro);
        if (selected != nullptr) {
            defined.push_back(*selected);
            continue;
        }
        if (declaration.optional) {
            continue;
        }
        if (declaration.options.size() == 1) {
            defined.push_back(ShaderMacroValue{.macro = declaration.macro, .value = declaration.options.front()});
            continue;
        }
        throw std::runtime_error(std::format("Failed to resolve shader variant: macro '{}' must be specified; it is not optional and has several options.", declaration.macro));
    }
    return make_key(std::move(defined));
}

} // namespace WhiteBirdEngine
