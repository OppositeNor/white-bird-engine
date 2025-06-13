#ifndef __WBE_PATH_H__
#define __WBE_PATH_H__

#include "platform/file_system/directory.h"
#include <string>

namespace WhiteBirdEngine {

class Path {
public:
    Path(const Directory& p_directory, const std::string& p_file_name)
        : directory(p_directory), file_name(p_file_name) {}
    Path(Directory&& p_directory, const std::string& p_file_name)
        : directory(std::move(p_directory)), file_name(p_file_name) {}

private:
    Directory directory;
    std::string file_name;
};

}

#endif
