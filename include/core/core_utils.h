#ifndef __CORE_UTILS_H__
#define __CORE_UTILS_H__

#include "global/global.h"
#include "logging/log.h"

#include <memory>
namespace WhiteBirdEngine {

template <typename T>
inline std::unique_ptr<Log>& log() {
    return Global::get_singleton()->logging_manager->get<T>();
}

}

#endif
