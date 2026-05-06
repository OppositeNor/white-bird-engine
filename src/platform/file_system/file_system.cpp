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
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace WhiteBirdEngine {

FileSystem::FileSystem() : FileSystem(get_executable_dir()) {
}

FileSystem::FileSystem(const Directory& p_root_dir) {
    root_directory = p_root_dir;
    resource_directory = root_directory.combine(Directory({"res"}));
    config_directory = resource_directory.combine(Directory({"config"}));
}

Directory FileSystem::parse_directory(const std::string& p_str) {
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

std::string FileSystem::get_file_name(const std::string& p_path) {
    return std::filesystem::path(p_path).filename().generic_string();
}

Directory FileSystem::get_file_dir(const std::string& p_path) {
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
#if defined(_WIN32)
    char buffer[1024];
    unsigned long len = ::GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
    if (len == 0) {
        throw std::runtime_error("Failed to get the executable path.");
    }
    if (len >= sizeof(buffer)) {
        throw std::runtime_error("Buffer overflow for executable path finding.");
    }
    buffer[len] = '\0';
    std::string exe_path(buffer);
    std::replace(exe_path.begin(), exe_path.end(), '\\', '/');
    return get_file_dir(exe_path);
#elif defined(__APPLE__)
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        throw std::runtime_error("Buffer overflow for executable path finding.");
    }
    return get_file_dir(std::filesystem::canonical(buffer).generic_string());
#else
    std::error_code ec;
    auto exe_path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        throw std::runtime_error("Failed to get the executable path.");
    }
    return get_file_dir(exe_path.generic_string());
#endif
}

} // namespace WhiteBirdEngine
