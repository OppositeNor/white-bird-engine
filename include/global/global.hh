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
#ifndef __WBE_GLOBAL_HH__
#define __WBE_GLOBAL_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/engine_core.hh"
#include "core/memory/unique.hh"
#include "function/engine_function.hh"
#include "platform/file_system/directory.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {
/**
 * @class Global
 * @brief Global class. Stores all the global objects.
 * Terminate and then reinitialize it should result in restarting the game/engine.
 */
class Global : public Singleton<Global> {
public:
    ~Global();

    /**
     * @brief Constructor.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     */
    Global(int p_argc, char* p_argv[]);

    /**
     * @brief Constructor.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     * @param p_root_dir The root directory.
     */
    Global(int p_argc, char* p_argv[], const Directory& p_root_dir);

    /**
     * @brief Get the singleton.
     *
     * @return The singleton instance.
     */
    static Global* get_singleton() {
        return singleton;
    }

    /**
     * @brief The core singleton.
     */
    EngineCore* engine_core;

    /**
     * @brief The function singleton.
     */
    Unique<EngineFunction> engine_function;

private:
    inline static Global* singleton;
};

/**
 * @brief Shortcut function to get the global allocator.
 *
 * @return The global allocator;
 */
inline HeapAllocatorAlignedPool* global_allocator() {
    return Global::get_singleton()->engine_core->pool_allocator;
}

}


#endif
