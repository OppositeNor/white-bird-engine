// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_SINGLETON_HH__
#define __WBE_SINGLETON_HH__

#include <atomic>
#include <cstdint>
#include <stdexcept>
namespace WhiteBirdEngine {

/**
 * @class Singleton
 * @brief Singleton class. This class ensures a class has only one instance.
 *
 * @tparam T The type of the singleton.
 */
template <typename T>
class Singleton {
public:
    Singleton() {
        if (instance_count.fetch_add(1, std::memory_order_acq_rel) != 0) {
            throw std::runtime_error("Failed to construct object: trying to construct multiple singletons.");
        }
    }

    virtual ~Singleton() {
        instance_count.fetch_sub(1, std::memory_order_acq_rel);
    }

private:
    inline static std::atomic<uint8_t> instance_count = 0;
};

}

#endif
