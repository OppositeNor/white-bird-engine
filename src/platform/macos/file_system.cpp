#include "platform/file_system/file_system.h"
#include "utils/utils.h"
#include <string>
#include <unistd.h>

namespace WhiteBirdEngine {

FileSystem::FileSystem() {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
    if (len == -1) {
        throw std::runtime_error("Failed to get the executable path.");
    }
    buf[len] = '\0';
    root_directory = parse_directory(std::string(buf));
    resource_directory = root_directory.combine(Directory({"res"}));
}

FileSystem::FileSystem(const Directory& p_root_dir) {
    root_directory = p_root_dir;
    resource_directory = root_directory.combine(Directory({"res"}));
}

Directory FileSystem::parse_directory(const std::string& p_str) {
    auto splitted = split_string(p_str, '/');
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
    return p_path.substr(last_slash);
}

Directory FileSystem::get_file_dir(const std::string& p_path) {
    auto last_slash = p_path.find_last_of('/');
    if (last_slash == std::string::npos) {
        Directory();
    }
    return Directory(parse_directory(p_path.substr(0, last_slash)));
}

std::string FileSystem::path_to_string(const Path& p_path) {
    return dir_to_string(p_path.get_directory()) + p_path.get_file_name();
}

std::string FileSystem::get_ext(const Path& p_path) {
    auto& file_name = p_path.get_file_name();
    return file_name.substr(file_name.find_last_of('.'));
}

}

