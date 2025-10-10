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
#ifndef __WBE_ENGINE_CORE_HH__
#define __WBE_ENGINE_CORE_HH__
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/engine_config/engine_config.hh"
#include "core/clock/clock.hh"
#include "utils/game_metadata.hh"
#include "core/logging/log_stream.hh"
#include "core/logging/logging_manager.hh"
#include "platform/file_system/file_system.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {

/**
 * @class EngineCore
 * @brief Core singleton of the engine. Contains all singletons of the core layer.
 *
 */
class EngineCore : public Singleton<EngineCore> {
public:
    ~EngineCore();

    /**
     * @brief Constructor.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     */
    EngineCore(int p_argc, char* p_argv[]);

    /**
     * @brief Constructor.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     * @param p_root_dir The root directory.
     */
    EngineCore(int p_argc, char* p_argv[], const Directory& p_root_dir);

    /**
     * @brief Get the singleton.
     *
     * @return The singleton of engine core.
     */
    static EngineCore* get_singleton() {
        return singleton;
    }

    /**
     * @brief The global clock starts recording when the engine core is constructed.
     */
    Clock* global_clock;

    /**
     * @brief File system.
     */
    FileSystem* file_system;
    /**
     * @brief Engine configuration.
     */
    EngineConfig* engine_config;
    /**
     * @brief Single tick stack allocator. Contents will be cleared every time a tick ends.
     */
    StackAllocator* single_tick_allocator;
    /**
     * @brief Global pool allocator.
     */
    HeapAllocatorAlignedPool* pool_allocator;
    /**
     * @brief Manager for logs.
     */
    LoggingManager<LogStream, std::ostream>* stdio_logging_manager;
    /**
     * @brief Manager for profiling.
     */
    class ProfilingManager* profiling_manager;

    /**
     * @brief Metadata of the game.
     */
    GameMetadata* game_metadata;

private:
    void parse_metadata(const Path& p_metadata_config_path);
    void initialize(int p_argc, char* p_argv[]);
    inline static EngineCore* singleton;
};

/**
 * @brief Shortcut function to get the global allocator.
 *
 * @return The global allocator;
 */
inline HeapAllocatorAlignedPool* global_allocator() {
    return EngineCore::get_singleton()->pool_allocator;
}

}

#endif
