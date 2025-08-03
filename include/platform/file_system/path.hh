#ifndef __WBE_PATH_H__
#define __WBE_PATH_H__

#include "platform/file_system/directory.h"
#include "utils/utils.h"
#include <string>

namespace WhiteBirdEngine {

/**
 * @class Path
 * @brief Path class.
 * @todo Test
 */
class Path {
public:
    Path() : directory(), file_name() {}
    virtual ~Path() {}
    Path(const Path& p_other)
        : directory(p_other.directory), file_name(p_other.file_name) {}
    Path(Path&& p_other)
        : directory(std::move(p_other.directory)), file_name(std::move(p_other.file_name)) {}
    Path& operator=(const Path& p_other) {
        directory = p_other.directory;
        file_name = p_other.file_name;
        return *this;
    }
    Path& operator=(Path&& p_other) {
        directory = std::move(p_other.directory);
        file_name = std::move(p_other.file_name);
        return *this;
    }

    /**
     * @brief Constructor.
     *
     * @param p_directory The directory of the file that the path is referencing to.
     * @param p_file_name The name of the file the path is referencing to.
     */
    Path(const Directory& p_directory, const std::string& p_file_name)
        : directory(p_directory), file_name(p_file_name) {}

    /**
     * @brief Constructor.
     *
     * @param p_directory The directory of the file that the path is referencing to.
     * @param p_file_name The name of the file the path is referencing to.
     */
    Path(Directory&& p_directory, const std::string& p_file_name)
        : directory(std::move(p_directory)), file_name(p_file_name) {}

    bool operator==(const Path& p_other) const {
        return directory == p_other.directory && file_name == p_other.file_name;
    }

    bool operator!=(const Path& p_other) const {
        return directory != p_other.directory || file_name != p_other.file_name;
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

    operator std::string() const;

private:
    Directory directory;
    std::string file_name;
};

inline std::ostream& operator<<(std::ostream& p_ostream, const Path& p_path) {
    return p_ostream << static_cast<std::string>(p_path);
}

}

#endif
