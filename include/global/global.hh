// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_GLOBAL_HH__
#define __WBE_GLOBAL_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/engine_core.hh"
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
    EngineFunction* engine_function;

private:
    inline static Global* singleton;
};

inline HeapAllocatorAlignedPool* global_allocator() {
    return Global::get_singleton()->engine_core->pool_allocator;
}

}


#endif
