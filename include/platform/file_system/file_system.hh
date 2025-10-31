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
#ifndef __WBE_FILE_SYSTEM_HH__
#define __WBE_FILE_SYSTEM_HH__

#include "platform/file_system/directory.hh"
#include "platform/file_system/path.hh"
#include "utils/interface/singleton.hh"
#include <string>
namespace WhiteBirdEngine {

/**
 * @class FileSystem
 * @brief Platform API for file systems.
 *
 */
class FileSystem : public Singleton<FileSystem> {
public:
    FileSystem();
    ~FileSystem() {}

    /**
     * @brief Constructor.
     *
     * @param p_root_dir The path of the root directory.
     */
    FileSystem(const Directory& p_root_dir);

    /**
     * @brief Parse a directory from a string.
     *
     * @param p_str The string to parse directory from.
     * @return The directory parsed.
     */
    static Directory parse_directory(const std::string& p_str);

    /**
     * @brief Get the directory of the executable file.
     *
     * @return The directory of the executable file.
     */
    const Directory& get_root_directory() const {
        return root_directory;
    }

    /**
     * @brief Get the directory of the resource directory.
     *
     * @return The resource directory.
     */
    const Directory& get_resource_directory() const {
        return resource_directory;
    }

    /**
     * @brief Get the directory of the config directory.
     *
     * @return The config directory.
     */
    const Directory& get_config_directory() const {
        return config_directory;
    }

    /**
     * @brief Cast the directory to a string.
     *
     * @return The string of the directory.
     */
    static std::string dir_to_string(const Directory& p_directory);

    /**
     * @brief Get the name of a file.
     *
     * @param p_path The path to the file.
     */
    static std::string get_file_name(const std::string& p_path);

    /**
     * @brief Get the directory of the file.
     *
     * @param p_path The path to the file.
     * @return The directory of the file.
     */
    static Directory get_file_dir(const std::string& p_path);

    /**
     * @brief Get the path from a string.
     *
     * @param p_path The string path.
     * @return The path instance.
     */
    Path get_file_path(const std::string& p_path) const {
        return Path(get_file_dir(p_path), get_file_name(p_path));
    }

    /**
     * @brief Cast a path to a string.
     *
     * @param p_path The path to cast to string.
     */
    static std::string path_to_string(const Path& p_path);

    /**
     * @brief Get the extension name of the file at path.
     *
     * @param p_path The path to the file.
     */
    static std::string get_ext(const Path& p_path);

    /**
     * @brief Get the directory of the executable.
     *
     * @return The directory of the executable.
     */
    static Directory get_executable_dir();

private:
    Directory root_directory;
    Directory resource_directory;
    Directory config_directory;
};

/**
 * @brief Combine a directory with a path.
 *
 * @param p_dir The directory to combine.
 * @param p_path The path to combine.
 * @return The combined path: "p_dir/p_path"
 */
inline Path combine(const Directory& p_dir, const Path& p_path) {
    return Path(p_dir.combine(p_path.get_directory()), p_path.get_file_name());
}

/**
 * @brief Combine one directory with another.
 *
 * @param p_dir1 The base directory.
 * @param p_dir2 The directory to combine.
 * @return The combined directory: "p_dir1/p_dir2"
 */
inline Directory combine(const Directory& p_dir1, const Directory& p_dir2) {
    return p_dir1.combine(p_dir2);
}

}

#endif
