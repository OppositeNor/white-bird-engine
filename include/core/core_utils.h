#ifndef __CORE_UTILS_H__
#define __CORE_UTILS_H__

#include "global/global.h"
#include "logging/log.h"

#include <memory>
namespace WhiteBirdEngine {

template <typename T>
inline std::unique_ptr<Log>& stdio_log() {
    return Global::get_singleton()->stdio_logging_manager->get<T>();
}

}

#endif
