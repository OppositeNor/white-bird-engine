#ifndef __WBE_GLOBAL_H__
#define __WBE_GLOBAL_H__

#include "core/allocator/heap_allocator_pool.h"
#include "core/allocator/stack_allocator.h"
#include "core/engine_config/engine_config.h"
#include "core/logging/log_stream.h"
#include "core/logging/logging_manager.h"
#include "utils/interface/singleton.h"
#include "platform/file_system/path.h"

namespace WhiteBirdEngine {
class Global : public Singleton<Global> {
public:
    ~Global();

    Global(char* p_argc[], int p_argv, const Path& p_config_file_path);

    // global objects
    std::unique_ptr<EngineConfig> engine_config;
    std::unique_ptr<StackAllocator> single_tick_allocator;
    std::unique_ptr<HeapAllocatorPool> pool_allocator;
    std::unique_ptr<LoggingManager<LogStream>> logging_manager;

    static Global* get_singleton();
};

}


#endif
