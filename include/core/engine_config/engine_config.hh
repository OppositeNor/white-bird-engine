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
#ifndef __WBE_ENGINE_CONFIG_HH__
#define __WBE_ENGINE_CONFIG_HH__

#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"
#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {

/**
 * @class EngineConfigOptions
 * @brief Engine configurations.
 *
 */
struct WBE_META(WBE_CONFIG_OPTION) EngineConfigOptions final : public Serializable {

    WBE_DECL_SERIALIZABLE(EngineConfigOptions)

    /**
     * @brief Name of the engine.
     */
    const char* engine_name = "White Bird Engine";
    /**
     * @brief Engine version major.
     */
    const uint32_t version_major = 0;
    /**
     * @brief Engine version minor.
     */
    const uint32_t version_minor = 0;
    /**
     * @brief Engine version patch.
     */
    const uint32_t version_patch = 1;

    /**
     * @brief The size of the tick stack.
     */
    WBE_META(WBE_REFLECT)
    size_t single_tick_stack_size = WBE_KiB(64);
    /**
     * @brief The size of the global memory pool.
     */
    WBE_META(WBE_REFLECT)
    size_t global_mem_pool_size = WBE_KiB(128);
    /**
     * @brief The size of the thread memory pool.
     */
    WBE_META(WBE_REFLECT)
    size_t thread_mem_pool_size = WBE_KiB(16);

    /**
     * @brief The utility name while running the program.
     */
    WBE_META(WBE_REFLECT)
    std::string utility_name;
};

/**
 * @class EngineConfig
 * @brief Engine configuration class.
 *
 */
class EngineConfig : public Singleton<EngineConfig> {
public:

    /**
     * @brief Constructor.
     *
     * @param p_config_file_path The path to the config file.
     * @param p_argc argc
     * @param p_argv argv
     */
    EngineConfig(const Path& p_config_file_path, int p_argc, char* p_argv[]) {
        parse_config_file(p_config_file_path);
        if (p_argc > 0) {
            parse_cla(p_argc, p_argv);
        }
    }
    virtual ~EngineConfig() {}
    EngineConfig(const EngineConfig&) = delete;
    EngineConfig(EngineConfig&&) = delete;
    EngineConfig& operator=(const EngineConfig&) = delete;
    EngineConfig& operator=(EngineConfig&&) = delete;

    /**
     * @brief Get the configuration options.
     *
     * @return The configuration options.
     */
    const EngineConfigOptions& get_config_options(){
        return config_options;
    }

private:
    EngineConfigOptions config_options;

    void parse_cla(int p_argc, char* p_argv[]) {
        // TODO
    }

    void parse_config_file(const Path& p_path) {
        // TODO
    }

};
}

#endif
