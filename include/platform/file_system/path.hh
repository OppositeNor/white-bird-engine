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
#ifndef WBE_FILE_PATH_HH
#define WBE_FILE_PATH_HH

#include "platform/file_system/directory.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <ostream>
#include <string>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class Path
 * @brief Path class. Thin wrapper around a directory plus a file name, backed
 * conceptually by std::filesystem::path semantics.
 */
class Path final {
public:
    Path() = default;
    ~Path() = default;
    Path(const Path& p_other) = default;
    Path(Path&& p_other) noexcept = default;
    Path& operator=(const Path& p_other) = default;
    Path& operator=(Path&& p_other) noexcept = default;

    /**
     * @brief Constructor.
     *
     * @param p_directory The directory of the file that the path is referencing
     * to.
     * @param p_file_name The name of the file the path is referencing to.
     */
    Path(const Directory& p_directory, const std::string& p_file_name) : directory(p_directory), file_name(p_file_name) {
    }

    /**
     * @brief Constructor.
     *
     * @param p_directory The directory of the file that the path is referencing
     * to.
     * @param p_file_name The name of the file the path is referencing to.
     */
    Path(Directory&& p_directory, const std::string& p_file_name)
        : directory(std::move(p_directory)), file_name(p_file_name) {
    }

    bool operator==(const Path& p_other) const {
        return directory == p_other.directory && file_name == p_other.file_name;
    }

    bool operator!=(const Path& p_other) const {
        return !(*this == p_other);
    }

    /**
     * @brief Get the directory of the path.
     *
     * @return The directory of the path.
     */
    const Directory& get_directory() const {
        return directory;
    }

    /**
     * @brief Get the file name of the path.
     *
     * @return The file name of the path.
     */
    const std::string& get_file_name() const {
        return file_name;
    }

    /**
     * @brief Get the extension of the file (without the leading dot).
     *
     * @return The extension of the file. Empty string if no extension.
     */
    std::string get_extension() const {
        size_t dot_pos = file_name.rfind('.');
        if (dot_pos == std::string::npos || dot_pos == 0 || dot_pos == file_name.length() - 1) {
            return "";
        }
        return file_name.substr(dot_pos + 1);
    }

    /**
     * @brief Build a std::filesystem::path equivalent to this path.
     *
     * @return The composed std::filesystem::path.
     */
    std::filesystem::path to_filesystem_path() const {
        return directory.get_path() / file_name;
    }

    /**
     * @brief Is the path absolute.
     *
     * @return True if the path is absolute. False otherwise.
     */
    bool is_absolute() const {
        return directory.get_is_absolute();
    }

    HashCode hash() const {
        return dynam_hash(static_cast<std::string>(*this).c_str());
    }

    operator std::string() const {
        return static_cast<std::string>(directory) + file_name;
    }

private:
    Directory directory;
    std::string file_name;
};

inline std::ostream& operator<<(std::ostream& p_ostream, const Path& p_path) {
    return p_ostream << static_cast<std::string>(p_path);
}

} // namespace WhiteBirdEngine

#endif
