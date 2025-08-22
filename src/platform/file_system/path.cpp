// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#include "platform/file_system/path.hh"
#include "platform/file_system/file_system.hh"

namespace WhiteBirdEngine {


Path::operator std::string() const {
    return FileSystem::path_to_string(*this);
}

}
