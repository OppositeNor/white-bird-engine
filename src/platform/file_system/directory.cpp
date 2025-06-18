#include "platform/file_system/directory.h"
#include "platform/file_system/file_system.h"

namespace WhiteBirdEngine {
Directory::operator std::string() const {
    return FileSystem::dir_to_string(*this);
}

}
