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
#include "core/script/script_value.hh"
#include "utils/defs.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

namespace {

constexpr std::array<std::string_view, 4> VECTOR_COMPONENT_KEYS = {"x", "y", "z", "w"};

// The smallest double above every int64_t value: 2^63.
constexpr double INT64_UPPER_BOUND = 9223372036854775808.0;

} // namespace

std::string_view ScriptValue::get_kind_name(ScriptValueKind p_kind) {
    switch (p_kind) {
    case ScriptValueKind::NIL:
        return "nil";
    case ScriptValueKind::BOOLEAN:
        return "boolean";
    case ScriptValueKind::INTEGER:
        return "integer";
    case ScriptValueKind::FLOAT:
        return "float";
    case ScriptValueKind::STRING:
        return "string";
    case ScriptValueKind::LIST:
        return "list";
    case ScriptValueKind::MAP:
        return "map";
    }
    throw std::runtime_error("Unknown script value kind.");
}

ScriptValue ScriptValue::make_boolean(bool p_value) {
    ScriptValue result;
    result.value = p_value;
    return result;
}

ScriptValue ScriptValue::make_integer(int64_t p_value) {
    ScriptValue result;
    result.value = p_value;
    return result;
}

ScriptValue ScriptValue::make_float(double p_value) {
    ScriptValue result;
    result.value = p_value;
    return result;
}

ScriptValue ScriptValue::make_string(std::string_view p_value) {
    ScriptValue result;
    result.value = std::string(p_value);
    return result;
}

ScriptValue ScriptValue::make_list() {
    ScriptValue result;
    result.value = List();
    return result;
}

ScriptValue ScriptValue::make_map() {
    ScriptValue result;
    result.value = Map();
    return result;
}

bool ScriptValue::as_boolean() const {
    if (get_kind() != ScriptValueKind::BOOLEAN) {
        throw_kind_mismatch("boolean");
    }
    return std::get<bool>(value);
}

int64_t ScriptValue::as_integer() const {
    if (get_kind() != ScriptValueKind::INTEGER) {
        throw_kind_mismatch("integer");
    }
    return std::get<int64_t>(value);
}

double ScriptValue::as_float() const {
    if (get_kind() == ScriptValueKind::INTEGER) {
        return static_cast<double>(std::get<int64_t>(value));
    }
    if (get_kind() != ScriptValueKind::FLOAT) {
        throw_kind_mismatch("float");
    }
    return std::get<double>(value);
}

const std::string& ScriptValue::as_string() const {
    if (get_kind() != ScriptValueKind::STRING) {
        throw_kind_mismatch("string");
    }
    return std::get<std::string>(value);
}

const ScriptValue::List& ScriptValue::as_list() const {
    if (get_kind() != ScriptValueKind::LIST) {
        throw_kind_mismatch("list");
    }
    return std::get<List>(value);
}

const ScriptValue::Map& ScriptValue::as_map() const {
    if (get_kind() != ScriptValueKind::MAP) {
        throw_kind_mismatch("map");
    }
    return std::get<Map>(value);
}

void ScriptValue::push_back(ScriptValue p_element) {
    if (is_nil()) {
        value = List();
    }
    if (get_kind() != ScriptValueKind::LIST) {
        throw_kind_mismatch("list");
    }
    std::get<List>(value).push_back(std::move(p_element));
}

void ScriptValue::set_entry(std::string_view p_key, ScriptValue p_entry) {
    if (is_nil() || (get_kind() == ScriptValueKind::LIST && std::get<List>(value).empty())) {
        value = Map();
    }
    if (get_kind() != ScriptValueKind::MAP) {
        throw_kind_mismatch("map");
    }
    Map& map = std::get<Map>(value);
    auto position = std::ranges::lower_bound(map, p_key, std::less<>(), &MapEntry::first);
    if (position != map.end() && position->first == p_key) {
        position->second = std::move(p_entry);
        return;
    }
    map.emplace(position, std::string(p_key), std::move(p_entry));
}

const ScriptValue* ScriptValue::find(std::string_view p_key) const {
    if (get_kind() != ScriptValueKind::MAP) {
        return nullptr;
    }
    const Map& map = std::get<Map>(value);
    auto position = std::ranges::lower_bound(map, p_key, std::less<>(), &MapEntry::first);
    if (position == map.end() || position->first != p_key) {
        return nullptr;
    }
    return &position->second;
}

bool ScriptValue::operator==(const ScriptValue& p_other) const {
    return value == p_other.value;
}

std::vector<std::string> ScriptValue::get_all_keys() const {
    std::vector<std::string> keys;
    if (get_kind() != ScriptValueKind::MAP) {
        return keys;
    }
    const Map& map = std::get<Map>(value);
    keys.reserve(map.size());
    for (const MapEntry& entry : map) {
        keys.push_back(entry.first);
    }
    return keys;
}

int64_t ScriptValue::float_to_exact_integer(double p_value) {
    if (!std::isfinite(p_value)) {
        throw std::runtime_error(std::format("Failed to get script value: float {} is not finite.", p_value));
    }
    if (std::trunc(p_value) != p_value) {
        throw std::runtime_error(std::format("Failed to get script value: float {} has a fractional part.", p_value));
    }
    if (p_value < -INT64_UPPER_BOUND || p_value >= INT64_UPPER_BOUND) {
        throw std::runtime_error(std::format("Failed to get script value: float {} exceeds the signed 64-bit range.", p_value));
    }
    return static_cast<int64_t>(p_value);
}

void ScriptValue::set_vector_components(const float* p_components, size_t p_count) {
    WBE_DEBUG_ASSERT(p_components != nullptr && p_count <= VECTOR_COMPONENT_KEYS.size());
    Map map;
    map.reserve(p_count);
    value = std::move(map);
    for (size_t i = 0; i < p_count; ++i) {
        set_entry(VECTOR_COMPONENT_KEYS[i], make_float(p_components[i]));
    }
}

const ScriptValue::List& ScriptValue::get_list_or_empty() const {
    static const List empty_list;
    if (get_kind() == ScriptValueKind::MAP && std::get<Map>(value).empty()) {
        return empty_list;
    }
    return as_list();
}

void ScriptValue::throw_kind_mismatch(std::string_view p_expected) const {
    throw std::runtime_error(
        std::format("Script value kind mismatch: expected {} but got {}.", p_expected, get_kind_name(get_kind())));
}

} // namespace WhiteBirdEngine
