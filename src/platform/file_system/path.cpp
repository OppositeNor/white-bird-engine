#include "platform/file_system/path.hh"
#include "platform/file_system/file_system.hh"

namespace WhiteBirdEngine {


Path::operator std::string() const {
    return FileSystem::path_to_string(*this);
}

}
