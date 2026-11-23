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
#include "function/render/i_rend_object_container.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "function/render/debug_data.hh"
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

namespace {

std::string desc_rows_to_string(const DebugDataRows& p_rows) {
    DebugDataArray rows;
    rows.reserve(p_rows.size());
    for (const DebugData& row : p_rows) {
        rows.push_back(row);
    }
    DebugData root;
    root.set(std::move(rows));
    return ParserJSON::write(root);
}

} // namespace

std::string IRendObjectContainer::to_string(RendAlloc* p_alloc) const {
    return desc_rows_to_string(desc_debug_data(p_alloc));
}

} // namespace WhiteBirdEngine
