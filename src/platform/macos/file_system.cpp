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
#include "platform/file_system/file_system.hh"
#include "platform/file_system/directory.hh"
#include "platform/file_system/path.hh"
#include "utils/utils.hh"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <mach-o/dyld.h>

namespace WhiteBirdEngine {
FileSystem::FileSystem() : FileSystem(get_executable_dir()) {
    singleton = this;
}

FileSystem::~FileSystem() {
    singleton = nullptr;
}

FileSystem::FileSystem(const Directory& p_root_dir) {
    root_directory = p_root_dir;
    resource_directory = root_directory.combine(Directory({"res"}));
    config_directory = resource_directory.combine(Directory({"config"}));
}

Directory FileSystem::parse_directory(std::string_view p_str) {
    auto splitted = split_string(p_str, '/');
    if (splitted.empty()) {
        return Directory();
    }
    std::vector<std::string> path_stack;
    path_stack.reserve(splitted.size());
    for (auto& dir_name : splitted) {
        if (dir_name == "..") {
            if (!path_stack.empty()) {
                path_stack.pop_back();
            }
            continue;
        }
        if (dir_name == "." || dir_name.empty()) {
            continue;
        }
        path_stack.push_back(dir_name);
    }
    return Directory(path_stack, !p_str.empty() && p_str[0] == '/');
}

std::string FileSystem::dir_to_string(const Directory& p_directory) {
    return static_cast<std::string>(p_directory);
}

std::string FileSystem::get_file_name(std::string_view p_path) {
    return std::filesystem::path(p_path).filename().generic_string();
}

Directory FileSystem::get_file_dir(std::string_view p_path) {
    auto last_slash = p_path.find_last_of('/');
    if (last_slash == std::string::npos) {
        return Directory();
    }
    return parse_directory(p_path.substr(0, last_slash + 1));
}

std::string FileSystem::path_to_string(const Path& p_path) {
    return dir_to_string(p_path.get_directory()) + p_path.get_file_name();
}

std::string FileSystem::get_ext(const Path& p_path) {
    return std::filesystem::path(p_path.get_file_name()).extension().generic_string();
}

Directory FileSystem::get_executable_dir() {
    std::error_code ec;
    auto exe_path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        throw std::runtime_error("Failed to get the executable path.");
    }
    return get_file_dir(exe_path.generic_string());
}

} // namespace WhiteBirdEngine
