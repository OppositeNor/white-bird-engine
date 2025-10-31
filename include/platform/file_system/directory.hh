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
#ifndef __WBE_DIRECTORY_HH__
#define __WBE_DIRECTORY_HH__

#include <ostream>
#include <string>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class Directory
 * @brief Directory class.
 */
class Directory {
public:

    Directory()
        : dir_names(), is_absolute(false) {
    }
    ~Directory() {}
    Directory(const Directory& p_other)
        : dir_names(p_other.dir_names), is_absolute(p_other.is_absolute) {}
    Directory(Directory&& p_other)
        : dir_names(std::move(p_other.dir_names)), is_absolute(p_other.is_absolute) {}
    Directory& operator=(const Directory& p_other);
    Directory& operator=(Directory&& p_other);

    bool operator==(const Directory& p_other) const;

    bool operator!=(const Directory& p_other) const {
        return !(*this == p_other);
    }

    /**
     * @brief Constructor.
     *
     * @param p_dir_names The names of the full path to the directory. The root should be
     * at index 0, and the followings should be ordered according to the depth.
     * @param p_is_absolute Is the directory absolute or relative.
     * By relative it means relative to the user current working directory.
     */
    Directory(std::vector<std::string> p_dir_names, bool p_is_absolute = false);

    /**
     * @brief Get the directory after combine this directory with another directory.
     *
     * @param p_other The directory to combine to this directory.
     * @return The combined directory.
     */
    Directory combine(const Directory& p_other) const;

    /**
     * @brief Get if this directory is absolute.
     *
     * @return True if the directory is absolute, false if the directory is relative.
     * By relative it means relative to the user current working directory.
     */
    bool get_is_absolute() const {
        return is_absolute;
    }

    /**
     * @brief Get the names of the path to the directory. The rooth directory will be at
     * index 0, and the followings will be ordered according to the depth.
     *
     * @return The names of the path to the directory.
     */
    const std::vector<std::string>& get_dir_names() const {
        return dir_names;
    }

    operator std::string() const;

private:
    std::vector<std::string> dir_names;
    bool is_absolute;
};

inline std::ostream& operator<<(std::ostream& p_ostream, const Directory& p_directory) {
    return p_ostream << static_cast<std::string>(p_directory);
}

inline Directory operator+(const Directory& p_first, const Directory& p_second) {
    return p_first.combine(p_second);
}

}

#endif
