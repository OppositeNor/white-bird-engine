#ifndef __WBE_FILE_SYSTEM_H__
#define __WBE_FILE_SYSTEM_H__

#include "platform/file_system/directory.h"
#include "platform/file_system/path.h"
#include "utils/interface/singleton.h"
namespace WhiteBirdEngine {

class FileSystem : public Singleton<FileSystem> {
public:
    FileSystem();
    ~FileSystem() {}

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
    static const Directory& get_exe_directory();

    /**
     * @brief Cast the directory to a string.
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

private:
    static Directory exe_directory;
};

}

#endif
