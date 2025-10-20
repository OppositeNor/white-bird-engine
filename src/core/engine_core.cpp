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
#include "core/engine_core.hh"
#include "core/clock/clock.hh"
#include "core/engine_config/engine_config.hh"
#include "core/profiling/profiling_manager.hh"
#include "core/parser/parser_json.hh"
#include "generated/label_manager.gen.hh"
#include <iostream>

namespace WhiteBirdEngine {

EngineCore::~EngineCore() {
    delete label_manager;
    delete profiling_manager;
    delete file_system;
    delete pool_allocator;
    delete stdio_logging_manager;
    delete engine_config;
    delete global_clock;
    singleton = nullptr;
}

EngineCore::EngineCore(int p_argc, char* p_argv[])
    : Singleton<EngineCore>() {
    global_clock = new Clock();
    file_system = new FileSystem();
    initialize(p_argc, p_argv);
}

EngineCore::EngineCore(int p_argc, char* p_argv[], const Directory& p_root_dir)
    : Singleton<EngineCore>() {
    global_clock = new Clock();
    file_system = new FileSystem(p_root_dir);
    initialize(p_argc, p_argv);
}

void EngineCore::parse_metadata(const Path& p_metadata_config_path) {
    ParserJSON parser;
    parser.parse(p_metadata_config_path);
}

void EngineCore::initialize(int p_argc, char* p_argv[]) {
    engine_config = new EngineConfig(Path(file_system->get_config_directory(), "engine_config.yaml"), p_argc, p_argv);
    pool_allocator = new HeapAllocatorAlignedPoolImplicitList(engine_config->get_config_options().global_mem_pool_size);
    parse_metadata(Path(file_system->get_resource_directory(), "metadata.json"));
    stdio_logging_manager = new LoggingManager<LogStream, std::ostream>(std::cout);
    profiling_manager = new ProfilingManager();
    label_manager = new LabelManager();
    singleton = this;
}

}

