# Copyright 2025 OppositeNor
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Jinja filters that describe reflected C++ types as Lua type annotations (LuaLS / EmmyLua)."""
import re
from typing import Any, Callable

LUA_NAMESPACE = "wbe"

_STRING_TYPES = {"std::string", "std::string_view", "const char *", "const char*"}
_BOOLEAN_TYPES = {"bool"}
_INTEGER_TYPES = {
    "int", "unsigned", "unsigned int", "short", "unsigned short", "long", "unsigned long", "long long", "unsigned long long",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "char", "unsigned char",
    "HashCode", "MemID",
}
_NUMBER_TYPES = {"float", "double"}
_GLM_TYPES = {"glm::vec2": "Vec2", "glm::vec3": "Vec3", "glm::vec4": "Vec4", "glm::quat": "Quat"}
_TEMPLATE_PATTERN = re.compile(r"^(?P<name>[\w:]+)\s*<(?P<argument>.*)>$")


def _strip(p_type: str) -> str:
    """Remove namespaces, cv-qualifiers, and references the annotation does not need."""
    result = p_type.strip()
    result = re.sub(r"\bconst\b", "", result)
    result = result.replace("&", "").strip()
    for prefix in ("::WhiteBirdEngine::", "WhiteBirdEngine::", "WBE::"):
        result = result.replace(prefix, "")
    return result.strip()


def lua_class_name(p_name: str) -> str:
    """The Lua annotation name of a reflected class or enum, e.g. `wbe.RendImageParamsVK`."""
    return f"{LUA_NAMESPACE}.{_strip(p_name).replace('::', '.')}"


def lua_type(p_type: str, p_class_names: list[str], p_enum_names: list[str]) -> str:
    """The Lua annotation type of a reflected C++ field type.

    Args:
        p_type: The C++ type as reflection spells it.
        p_class_names: Names of the reflected classes the annotations declare.
        p_enum_names: Names of the reflected enums the annotations declare.
    """
    stripped = _strip(p_type)
    match = _TEMPLATE_PATTERN.match(stripped)
    if match is not None:
        name = match.group("name")
        argument = match.group("argument")
        if name in ("std::vector", "Vector"):
            element = lua_type(argument.split(",")[0], p_class_names, p_enum_names)
            return f"({element})[]" if "|" in element else f"{element}[]"
        if name == "std::optional":
            return lua_type(argument, p_class_names, p_enum_names)
        if name == "Buffer":
            return "string"
    if stripped in _STRING_TYPES:
        return "string"
    if stripped in _BOOLEAN_TYPES:
        return "boolean"
    if stripped in _INTEGER_TYPES:
        return "integer"
    if stripped in _NUMBER_TYPES:
        return "number"
    if stripped in _GLM_TYPES:
        return f"{LUA_NAMESPACE}.{_GLM_TYPES[stripped]}"
    if stripped == "glm::mat4":
        return "number[]"
    if stripped == "DataValue":
        return "any"
    if stripped in p_enum_names or stripped in p_class_names:
        return lua_class_name(stripped)
    return "any"


def lua_is_optional(p_type: str) -> bool:
    """Whether a reflected C++ field type is a std::optional."""
    return _strip(p_type).startswith("std::optional<")


def lua_task_params_type(p_method_type: list[str]) -> str:
    """The parameter struct name of a render task's create(), from its reflected parameter types."""
    if len(p_method_type) < 2:
        return ""
    return _strip(p_method_type[1])


LUA_FILTERS: dict[str, Callable[..., Any]] = {
    "lua_type": lua_type,
    "lua_class_name": lua_class_name,
    "lua_is_optional": lua_is_optional,
    "lua_task_params_type": lua_task_params_type,
}
