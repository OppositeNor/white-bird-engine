#include "platform/file_system/path.h"
#include "platform/file_system/file_system.h"

namespace WhiteBirdEngine {


Path::operator std::string() const {
    return FileSystem::path_to_string(*this);
}

}
