#include "platform/file_system/directory.h"
#include "platform/file_system/file_system.h"

namespace WhiteBirdEngine {
Directory::Directory(const std::string& p_dir_path) {
    *this = FileSystem::parse_directory(p_dir_path);
}

Directory::operator std::string() const {
    return FileSystem::dir_to_string(*this);
}

}
