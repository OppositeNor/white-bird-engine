#include "utils/defs.h"
#include "platform/file_system/directory.h"
#include "platform/file_system/file_system.h"
#include <cstdint>

namespace WhiteBirdEngine {

Directory::operator std::string() const {
    return FileSystem::dir_to_string(*this);
}

Directory& Directory::operator=(const Directory& p_other) {
    dir_names = p_other.dir_names;
    is_absolute = p_other.is_absolute;
    return *this;
}

Directory& Directory::operator=(Directory&& p_other) {
    dir_names = std::move(p_other.dir_names);
    is_absolute = p_other.is_absolute;
    return *this;
}

bool Directory::operator==(const Directory& p_other) const {
    if (is_absolute != p_other.is_absolute || dir_names.size() != p_other.dir_names.size()) {
        return false;
    }
    size_t dir_names_size = dir_names.size();
    for (uint32_t i = 0; i < dir_names_size; ++i) {
        if (dir_names[i] != p_other.dir_names[i]) {
            return false;
        }
    }
    return true;
}

bool Directory::operator!=(const Directory& p_other) const {
    return !(*this == p_other);
}

Directory::Directory(std::vector<std::string> p_dir_names, bool p_is_absolute)
: dir_names(p_dir_names), is_absolute(p_is_absolute) {}


Directory Directory::combine(const Directory& p_other) const {
    WBE_DEBUG_ASSERT(!p_other.get_is_absolute());
    Directory result(*this);
    result.dir_names.reserve(dir_names.size() + p_other.dir_names.size());
    for (auto& dir_name : p_other.dir_names) {
        result.dir_names.push_back(dir_name);
    }
    return result;
}

bool Directory::get_is_absolute() const {
    return is_absolute;
}

const std::vector<std::string>& Directory::get_dir_names() const {
    return dir_names;
}

}
