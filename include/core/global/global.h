#ifndef __WBE_GLOBAL_H__
#define __WBE_GLOBAL_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/allocator/stack_allocator.h"
#include "core/engine_config/engine_config.h"
#include "core/logging/log_stream.h"
#include "core/logging/logging_manager.h"
#include "core/profiling/profiling_manager.h"
#include "platform/file_system/file_system.h"
#include "utils/interface/singleton.h"
#include "platform/file_system/path.h"
#include <ostream>

namespace WhiteBirdEngine {
/**
 * @class Global
 * @brief Global class. Stores all the global objects.
 * Terminate and then reinitialize it should result in restarting the game/engine.
 */
class Global : public Singleton<Global> {
private:
    Global(int p_argc, char* p_argv[], const Path& p_config_file_path);
    ~Global();
public:

    /**
     * @brief Initialize global objects.
     *
     * @param p_argc The number of command line arguments.
     * @param p_argv The command line arguments.
     * @param p_config_file_path The path to the configuration file.
     */
    static void init(int p_argc, char* p_argv[], const Path& p_config_file_path);

    /**
     * @brief Free all the global resources and terminate to the uninitialized state.
     */
    static void terminate();

    /**
     * @brief Get the singleton.
     *
     * @return The singleton instance.
     */
    static Global* get_singleton();

    /**
     * @brief Engine configuration.
     */
    std::unique_ptr<EngineConfig> engine_config;
    /**
     * @brief Single tick stack allocator. Contents will be cleared every time a tick ends.
     */
    std::unique_ptr<StackAllocator> single_tick_allocator;
    /**
     * @brief Global pool allocator.
     */
    std::unique_ptr<HeapAllocatorPool> pool_allocator;
    /**
     * @brief Manager for logs.
     */
    std::unique_ptr<LoggingManager<LogStream, std::ostream>> stdio_logging_manager;
    /**
     * @brief Manager for profiling.
     */
    std::unique_ptr<ProfilingManager> profiling_manager;

private:
    inline static Global* singleton;
};

}


#endif
