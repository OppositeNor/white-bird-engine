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
#include "core/parser/parser_toml.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace WhiteBirdEngine {

namespace {

[[noreturn]] void throw_parse_error(const toml::parse_error& p_error, std::string_view p_source) {
    throw std::runtime_error(std::format("Failed to parse TOML from {} at line {}, column {}: {}",
        p_source,
        p_error.source().begin.line,
        p_error.source().begin.column,
        p_error.description()));
}

} // namespace

void ParserTOML::parse(const Path& p_path) {
    toml::parse_result result = toml::parse_file(p_path.string());
    if (!result) {
        throw_parse_error(result.error(), p_path.string());
    }
    data = TOMLData(std::move(result).table());
}

void ParserTOML::parse_from_buffer(std::string_view p_buffer) {
    toml::parse_result result = toml::parse(p_buffer);
    if (!result) {
        throw_parse_error(result.error(), "buffer");
    }
    data = TOMLData(std::move(result).table());
}

} // namespace WhiteBirdEngine
