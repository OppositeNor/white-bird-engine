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
"""Jinja filters that describe reflected C++ types as Teal declarations, nested in the `wbe` declaration record."""
import re
from typing import Any, Callable


def _strip(p_type: str) -> str:
    """Remove namespaces, cv-qualifiers, and references the declarations do not need."""
    result = p_type.strip()
    result = re.sub(r"\bconst\b", "", result)
    result = result.replace("&", "").strip()
    for prefix in ("::WhiteBirdEngine::", "WhiteBirdEngine::", "WBE::"):
        result = result.replace(prefix, "")
    return result.strip()


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
_IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z_]\w*$")
# Lua keywords, and the words Teal reads as declarations inside a record body.
_RESERVED_FIELD_NAMES = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto", "if", "in", "local", "nil", "not",
    "or", "repeat", "return", "then", "true", "until", "while", "global", "record", "interface", "enum", "type", "where",
    "is", "as", "userdata", "metamethod",
}


def teal_name(p_name: str) -> str:
    """The Teal name of a reflected class or enum inside the `wbe` record, e.g. `RendImageParamsVK`."""
    return _strip(p_name).replace("::", "_")


def teal_field_name(p_name: str) -> str:
    """A record field name, quoted when Teal would read it as a keyword."""
    if p_name in _RESERVED_FIELD_NAMES or not _IDENTIFIER_PATTERN.match(p_name):
        return f'["{p_name}"]'
    return p_name


def teal_type(p_type: str, p_scope: dict[str, Any]) -> str:
    """The Teal type of a reflected C++ field type.

    Reflected enums accept their names and aliases, or their integer values, as deserialization does.

    Args:
        p_type: The C++ type as reflection spells it.
        p_scope: The names the declarations can refer to: `classes` and `enums`, the script types they declare, and
            `reflected`, every reflected class and enum, whether visible to scripts or not.

    Raises:
        ValueError: If the type is a reflected class or enum that is not visible to scripts.
    """
    stripped = _strip(p_type)
    match = _TEMPLATE_PATTERN.match(stripped)
    if match is not None:
        name = match.group("name")
        argument = match.group("argument")
        if name in ("std::vector", "Vector"):
            return f"{{{teal_type(argument.split(',')[0], p_scope)}}}"
        if name == "std::optional":
            return teal_type(argument, p_scope)
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
        return _GLM_TYPES[stripped]
    if stripped == "glm::mat4":
        return "{number}"
    if stripped == "DataValue":
        return "any"
    if stripped in p_scope["enums"]:
        return f"{teal_name(stripped)} | integer"
    if stripped in p_scope["classes"]:
        return teal_name(stripped)
    if stripped in p_scope["reflected"]:
        raise ValueError(f"The reflected type '{stripped}' reaches scripts but is not WBE_REFLECT_SCRIPT; mark it so it is declared.")
    return "any"


def teal_parent(p_parent: str, p_scope: dict[str, Any]) -> str:
    """The Teal name of the interface a script class derives from, or an empty string for a base that is not reflected,
    which scripts never see.

    Raises:
        ValueError: If the base is a reflected class that is not visible to scripts.
    """
    stripped = _strip(p_parent)
    if stripped in p_scope["classes"]:
        return teal_name(stripped)
    if stripped in p_scope["reflected"]:
        raise ValueError(f"The script class base '{stripped}' is not WBE_REFLECT_SCRIPT; mark it so it is declared.")
    return ""


TEAL_FILTERS: dict[str, Callable[..., Any]] = {
    "teal_type": teal_type,
    "teal_name": teal_name,
    "teal_field_name": teal_field_name,
    "teal_parent": teal_parent,
}
