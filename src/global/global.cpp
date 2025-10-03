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
#include "global/global.hh"
#include "core/engine_core.hh"
#include "function/engine_function.hh"
#include "platform/os/os.hh"

namespace WhiteBirdEngine {

Global::Global(int p_argc, char* p_argv[])
    : Singleton<Global>() {
    singleton = this;
    engine_core = new EngineCore(p_argc, p_argv);
    auto* file_system = Global::get_singleton()->engine_core->file_system;
    engine_function = make_unique<EngineFunction>(global_allocator(), Path(file_system->get_config_directory(), "game_config.yaml"));
    OS::wait_all();
}

Global::Global(int p_argc, char* p_argv[], const Directory& p_root_dir)
    : Singleton<Global>() {
    singleton = this;
    engine_core = new EngineCore(p_argc, p_argv, p_root_dir);
    auto* file_system = Global::get_singleton()->engine_core->file_system;
    engine_function = make_unique<EngineFunction>(global_allocator(), Path(file_system->get_config_directory(), "game_config.yaml"));
}

Global::~Global() {
    engine_function.reset();
    delete engine_core;
    singleton = nullptr;
}


}
