#ifndef __WBE_RESOURCE_UTILS_H__
#define __WBE_RESOURCE_UTILS_H__

#include "core/engine_core.h"
#include "platform/file_system/directory.h"
#include "platform/file_system/file_system.h"
#include "platform/file_system/path.h"
#include <string>
namespace WhiteBirdEngine {

/**
 * @brief Parse a resource path relative to the resource directory.
 *
 * @todo Test
 * @param p_res_path The resource path.
 * @return The resource path relative to the resource directory.
 */
inline Path parse_res_rel_path(const std::string& p_res_path) {
    auto splitted = split_string(p_res_path, '/');
    std::string file_name = splitted.back();
    splitted.pop_back();
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
    return Path(Directory(path_stack, false), file_name);
}

/**
 * @brief Parse a resource path.
 *
 * @todo Test
 * @param p_res_path The resource path.
 * @return The path to the resource in the filesystem.
 */
inline Path get_resource_path(const std::string& p_res_path) {
    Directory res_directory = EngineCore::get_singleton()->file_system->get_resource_directory();
    return combine(res_directory, parse_res_rel_path(p_res_path));
}

}

#endif
