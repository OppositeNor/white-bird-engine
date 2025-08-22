/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
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
