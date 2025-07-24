#include "platform/file_system/file_system.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <windows.h>

namespace WhiteBirdEngine {
FileSystem::FileSystem()
    : FileSystem(get_executable_dir()) {
}

FileSystem::FileSystem(const Directory& p_root_dir) {
    root_directory = p_root_dir;
    resource_directory = root_directory.combine(Directory({"res"}));
}

Directory FileSystem::parse_directory(const std::string& p_str) {
    if (p_str.size() == 0) {
        return Directory();
    }
    auto splitted = split_string(p_str, "/\\");
    std::vector<std::string> path_stack;
    path_stack.reserve(splitted.size());
    auto front = path_stack.front();
    bool is_absolute = front.size() == 2 && front[1] == ':';
    if (std::count(p_str.begin(), p_str.end(), ':') > is_absolute ? 1 : 0) {
        throw std::runtime_error("Failed to parse directory: volume indicator ':' not used properly in the path: " + p_str + ".");
    }
    for (auto& dir_name : splitted) {
        if (dir_name == "..") {
            if (path_stack.size() > is_absolute ? 1 : 0) {
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
    auto& dir_names = p_directory.get_dir_names();
    std::stringstream ss;
    for (auto& dir_name : dir_names) {
        ss << dir_name << '\\';
    }
    return ss.str();
}

std::string FileSystem::get_file_name(const std::string& p_path) {
    return ""; // TODO
}

Directory FileSystem::get_file_dir(const std::string& p_path) {
    return Directory(); // TODO
}

std::string FileSystem::path_to_string(const Path& p_path) {
    // TODO: Test
    return dir_to_string(p_path.get_directory()) + p_path.get_file_name();
}

std::string FileSystem::get_ext(const Path& p_path) {
    // TODO: Test
    auto& file_name = p_path.get_file_name();
    auto ext_result = file_name.substr(file_name.find_last_of('.'));
    std::for_each(ext_result.begin(), ext_result.end(), [](char& p_c) { p_c = std::tolower(p_c); });
    return ext_result;
}

Directory FileSystem::get_executable_dir() {
    char buf[1024];
    ssize_t len = GetModuleFileName(NULL, buf, sizeof(buf));
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

