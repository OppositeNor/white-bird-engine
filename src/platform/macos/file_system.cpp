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
#include "utils/utils.hh"
#include <string>
#include <unistd.h>

namespace WhiteBirdEngine {

FileSystem::FileSystem()
    : FileSystem(get_executable_dir()) {
}

FileSystem::FileSystem(const Directory& p_root_dir) {
    root_directory = p_root_dir;
    resource_directory = root_directory.combine(Directory({"res"}));
    config_directory = resource_directory.combine(Directory({"config"}));
}

Directory FileSystem::parse_directory(const std::string& p_str) {
    auto splitted = split_string(p_str, '/');
    if (splitted.size() == 0) {
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
        if (dir_name == "." || dir_name == "") {
            continue;
        }
        path_stack.push_back(dir_name);
    }
    return Directory(path_stack, p_str.size() != 0 && p_str[0] == '/');
}

std::string FileSystem::dir_to_string(const Directory& p_directory) {
    std::string result = p_directory.get_is_absolute() ? "/" : "";
    auto& dir_names = p_directory.get_dir_names();
    for (auto& dir_name : dir_names) {
        result += dir_name + "/";
    }
    return result;
}

std::string FileSystem::get_file_name(const std::string& p_path) {
    auto last_slash = p_path.find_last_of('/');
    if (last_slash == std::string::npos) {
        return p_path;
    }
    return p_path.substr(last_slash + 1);
}

Directory FileSystem::get_file_dir(const std::string& p_path) {
    auto last_slash = p_path.find_last_of('/');
    if (last_slash == std::string::npos) {
        Directory();
    }
    std::string dir_str = p_path.substr(0, last_slash + 1);
    return Directory(parse_directory(dir_str));
}

std::string FileSystem::path_to_string(const Path& p_path) {
    return dir_to_string(p_path.get_directory()) + p_path.get_file_name();
}

std::string FileSystem::get_ext(const Path& p_path) {
    auto& file_name = p_path.get_file_name();
    size_t ext_dest = file_name.find_last_of('.');
    if (ext_dest == 0 || ext_dest == std::string::npos) {
        return "";
    }
    return file_name.substr(ext_dest);
}

Directory FileSystem::get_executable_dir() {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
    if (len < 0) {
        throw std::runtime_error("Failed to get the executable path.");
    }
    if (len >= 1024) {
        throw std::runtime_error("Buffer overflow for executable path finding.");
    }
    buf[len] = '\0';
    return parse_directory(std::string(buf));
}


}

