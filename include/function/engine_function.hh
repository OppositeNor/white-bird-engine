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
#ifndef __WBE_ENGINE_FUNCTION_HH__
#define __WBE_ENGINE_FUNCTION_HH__

#include "core/allocator/allocator.hh"
#include "core/memory/unique.hh"
#include "function/game_config/game_config.hh"
#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {

/**
 * @class EngineFunction
 * @brief The manager for the function layer.
 *
 */
class EngineFunction final : public Singleton<EngineFunction>{
public:
    EngineFunction() = delete;
    virtual ~EngineFunction() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(EngineFunction)

    /**
     * @brief Constructor.
     *
     * @param p_game_config_file_path The path to the game configurations.
     */
    EngineFunction(const Path& p_game_config_file_path);

    /**
     * @brief Get the singleton instance.
     *
     * @return The singleton instance.
     */
    static EngineFunction* get_singleton() {
        return singleton;
    }

    /**
     * @brief The game configuration.
     */
    Unique<GameConfig> game_config = MEM_NULL;
private:

    inline static EngineFunction* singleton;
};

} // namespace WhiteBirdEngine

#endif
