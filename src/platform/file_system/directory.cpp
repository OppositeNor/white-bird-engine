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
#include "platform/file_system/directory.hh"
#include "platform/file_system/file_system.hh"
#include <cstdint>

namespace WhiteBirdEngine {

Directory::operator std::string() const {
    return FileSystem::dir_to_string(*this);
}

Directory& Directory::operator=(const Directory& p_other) {
    dir_names = p_other.dir_names;
    is_absolute = p_other.is_absolute;
    return *this;
}

Directory& Directory::operator=(Directory&& p_other) {
    dir_names = std::move(p_other.dir_names);
    is_absolute = p_other.is_absolute;
    return *this;
}

bool Directory::operator==(const Directory& p_other) const {
    if (is_absolute != p_other.is_absolute || dir_names.size() != p_other.dir_names.size()) {
        return false;
    }
    size_t dir_names_size = dir_names.size();
    for (uint32_t i = 0; i < dir_names_size; ++i) {
        if (dir_names[i] != p_other.dir_names[i]) {
            return false;
        }
    }
    return true;
}

Directory::Directory(std::vector<std::string> p_dir_names, bool p_is_absolute)
: dir_names(p_dir_names), is_absolute(p_is_absolute) {}


Directory Directory::combine(const Directory& p_other) const {
    if (p_other.is_absolute) {
        throw std::runtime_error("Only allows to combine with a relative path.");
    }
    Directory result(*this);
    result.dir_names.reserve(dir_names.size() + p_other.dir_names.size());
    for (auto& dir_name : p_other.dir_names) {
        result.dir_names.push_back(dir_name);
    }
    return result;
}

}
