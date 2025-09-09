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

#include "function/game_config/game_config.hh"
#include "platform/file_system/path.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {

class EngineFunction : public Singleton<EngineFunction>{
public:
    ~EngineFunction();

    EngineFunction(const Path& p_game_config_file_path);

    static EngineFunction* get_singleton() {
        return singleton;
    }

    GameConfig* game_config = nullptr;
private:

    inline static EngineFunction* singleton;
};

}

#endif
