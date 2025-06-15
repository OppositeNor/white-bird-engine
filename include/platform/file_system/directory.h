#ifndef __WBE_DIRECTORY_H__
#define __WBE_DIRECTORY_H__

#include "utils/defs.h"

#include <cstdint>
#include <string>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class Directory
 * @brief Directory class.
 * @todo Test
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
    Directory& operator=(const Directory& p_other) {
        dir_names = p_other.dir_names;
        is_absolute = p_other.is_absolute;
        return *this;
    }
    Directory& operator=(Directory&& p_other) {
        dir_names = std::move(p_other.dir_names);
        is_absolute = p_other.is_absolute;
        return *this;
    }

    bool operator==(const Directory& p_other) const {
        if (is_absolute != p_other.is_absolute || dir_names.size() != p_other.dir_names.size()) {
            return false;
        }
        for (uint32_t i = 0; i < dir_names.size(); ++i) {
            if (dir_names[i] != p_other.dir_names[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const Directory& p_other) const {
        return !(*this == p_other);
    }


    /**
     * @brief Constructor.
     *
     * @param p_dir_names The names of the full path to the directory.
     * @param p_is_absolute Is the directory absolute or relative.
     * By relative it means relative to the user current working directory.
     */
    Directory(std::vector<std::string> p_dir_names, bool p_is_absolute)
        : dir_names(p_dir_names), is_absolute(p_is_absolute) {}


    /**
     * @brief Get the directory after combine this directory with another directory.
     *
     * @param p_other The directory to combine to this directory.
     * @todo Test
     * @return The combined directory.
     */
    Directory combine(const Directory& p_other) const {
        WBE_DEBUG_ASSERT(!p_other.get_is_absolute());
        Directory result(*this);
        result.dir_names.reserve(dir_names.size() + p_other.dir_names.size());
        for (auto& dir_name : p_other.dir_names) {
            result.dir_names.push_back(dir_name);
        }
        return result;
    }

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
     * @brief Get the names of the path to the directory.
     *
     * @return The names of the path to the directory.
     */
    const std::vector<std::string>& get_dir_names() const {
        return dir_names;
    }

private:
    std::vector<std::string> dir_names;
    bool is_absolute;
};

}

#endif
