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
#ifndef WBE_FILE_DIRECTORY_HH
#define WBE_FILE_DIRECTORY_HH

#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class Directory
 * @brief Directory class. Thin wrapper around std::filesystem::path.
 */
class Directory {
public:
    Directory() = default;
    ~Directory() = default;
    Directory(const Directory& p_other) = default;
    Directory(Directory&& p_other) noexcept = default;
    Directory& operator=(const Directory& p_other) = default;
    Directory& operator=(Directory&& p_other) noexcept = default;

    /**
     * @brief Constructor.
     *
     * @param p_dir_names The names of the full path to the directory. The root
     * should be at index 0, and the followings should be ordered according to
     * the depth.
     * @param p_is_absolute Is the directory absolute or relative.
     * By relative it means relative to the user current working directory.
     */
    Directory(const std::vector<std::string>& p_dir_names, bool p_is_absolute = false) {
        if (p_is_absolute) {
            inner = std::filesystem::path("/");
        }
        for (const auto& name : p_dir_names) {
            inner /= name;
        }
    }

    bool operator==(const Directory& p_other) const {
        return inner == p_other.inner;
    }

    bool operator!=(const Directory& p_other) const {
        return !(*this == p_other);
    }

    /**
     * @brief Get the directory after combine this directory with another
     * directory.
     *
     * @param p_other The directory to combine to this directory.
     * @return The combined directory.
     */
    Directory combine(const Directory& p_other) const {
        if (p_other.get_is_absolute()) {
            throw std::runtime_error("Only allows to combine with a relative path.");
        }
        Directory result(*this);
        for (const auto& component : p_other.inner) {
            result.inner /= component;
        }
        return result;
    }

    /**
     * @brief Get if this directory is absolute.
     *
     * @return True if the directory is absolute, false if the directory is
     * relative. By relative it means relative to the user current working
     * directory.
     */
    bool get_is_absolute() const {
        return inner.is_absolute();
    }

    /**
     * @brief Get the names of the path to the directory. The root directory
     * will be at index 0, and the followings will be ordered according to the
     * depth.
     *
     * @return The names of the path to the directory.
     */
    std::vector<std::string> get_dir_names() const {
        std::vector<std::string> result;
        for (const auto& component : inner) {
            std::string s = component.generic_string();
            if (s.empty() || s == "/") {
                continue;
            }
            result.push_back(std::move(s));
        }
        return result;
    }

    /**
     * @brief Get the underlying std::filesystem::path.
     *
     * @return The wrapped path.
     */
    const std::filesystem::path& get_path() const {
        return inner;
    }

    operator std::string() const {
        std::string s = inner.generic_string();
        if (s.empty() || s.back() == '/') {
            return s;
        }
        return s + "/";
    }

private:
    std::filesystem::path inner;
};

inline std::ostream& operator<<(std::ostream& p_ostream, const Directory& p_directory) {
    return p_ostream << static_cast<std::string>(p_directory);
}

inline Directory operator+(const Directory& p_first, const Directory& p_second) {
    return p_first.combine(p_second);
}

} // namespace WhiteBirdEngine

#endif
