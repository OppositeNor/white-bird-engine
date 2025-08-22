#ifndef __WBE_ENGINE_CORE_HH__
#define __WBE_ENGINE_CORE_HH__
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/engine_config/engine_config.hh"
#include "utils/game_metadata.hh"
#include "core/logging/log_stream.hh"
#include "core/logging/logging_manager.hh"
#include "core/profiling/profiling_manager.hh"
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
    ProfilingManager* profiling_manager;

    /**
     * @brief Metadata of the game.
     */
    GameMetadata* game_metadata;

private:
    void parse_metadata(const Path& p_metadata_config_path);
    void initialize(int p_argc, char* p_argv[]);
    inline static EngineCore* singleton;
};

}

#endif
