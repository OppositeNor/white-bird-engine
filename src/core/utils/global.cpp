#include "function/global/global.h"
#include "utils/game_metadata.h"
#include "core/logging/log_stream.h"
#include "core/parser/parser_json.h"
#include "platform/file_system/file_system.h"
#include <iostream>

namespace WhiteBirdEngine {

Global::Global(int p_argc, char* p_argv[])
    : Singleton<Global>() {
    singleton = this;
    file_system = new FileSystem();
    initialize(p_argc, p_argv);
}

Global::Global(int p_argc, char* p_argv[], const Directory& p_root_dir) {
    singleton = this;
    file_system = new FileSystem(p_root_dir);
    initialize(p_argc, p_argv);
}

Global::~Global() {
    delete stdio_logging_manager;
    delete game_metadata;
    delete file_system;
}

Global* Global::get_singleton() {
    return singleton;
}

void Global::parse_metadata(const Path& p_metadata_config_path) {
    ParserJSON parser;
    parser.parse(p_metadata_config_path);
    auto channels_data = parser.get_value<JSONData>("channels");
    auto channel_keys = channels_data.get_all_keys();
    for (auto& key : channel_keys) {
        game_metadata->get_channel_metadata().add_pair(key, channels_data.get_value<ChannelID>(key));
    }
}

void Global::initialize(int p_argc, char* p_argv[]) {
    game_metadata = new GameMetadata();
    parse_metadata(Path(singleton->file_system->get_resource_directory(), "metadata.json"));
    stdio_logging_manager = new LoggingManager<LogStream, std::ostream>(std::cout);
}

}
