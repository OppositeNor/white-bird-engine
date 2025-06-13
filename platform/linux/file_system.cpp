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
    exe_directory = parse_directory(std::string(buf));
}

Directory FileSystem::parse_directory(const std::string& p_str) {
    std::string str = p_str;
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
    return Directory(path_stack, p_str[0] == '/');
}

std::string FileSystem::dir_to_string(const Directory& p_directory) {
    std::string result = p_directory.get_is_absolute() ? "/" : "";
    auto& dir_names = p_directory.get_dir_names();
    for (auto& dir_name : dir_names) {
        result += dir_name + "/";
    }
    return result;
}

}

