#include "platform/file_system/file_system.h"
#include <algorithm>
#include <cctype>

namespace WhiteBirdEngine {
FileSystem::FileSystem() {
    // TODO
}

Directory FileSystem::parse_directory(const std::string& p_str) {
    return Directory(); // TODO
}

std::string FileSystem::dir_to_string(const Directory& p_directory) {
    return ""; // TODO
}

std::string FileSystem::get_file_name(const std::string& p_path) {
    return ""; // TODO
}

Directory FileSystem::get_file_dir(const std::string& p_path) {
    return Directory(); // TODO
}

std::string FileSystem::path_to_string(const Path& p_path) {
    return std::string(); // TODO
}

std::string FileSystem::get_ext(const Path& p_path) {
    auto& file_name = p_path.get_file_name();
    auto ext_result = file_name.substr(file_name.find_last_of('.'));
    std::for_each(ext_result.begin(), ext_result.end(), [](char& p_c) { p_c = std::tolower(p_c); });
    return ext_result;
}

}

