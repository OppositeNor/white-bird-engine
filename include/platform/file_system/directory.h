#ifndef __WBE_DIRECTORY_H__
#define __WBE_DIRECTORY_H__

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

    bool operator==(const Directory& p_other) {
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

    bool operator!=(const Directory& p_other) {
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


    // TODO: Other methods

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
