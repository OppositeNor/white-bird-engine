#include "utils/global/global.h"
#include "utils/game_metadata.h"
#include "core/logging/log_stream.h"
#include "core/parser/parser_json.h"
#include "platform/file_system/file_system.h"
#include <iostream>
#include <memory>

namespace WhiteBirdEngine {

Global::Global(int p_argc, char* p_argv[])
    : Singleton<Global>() {
    singleton = this;
    file_system = std::make_unique<FileSystem>();
    initialize(p_argc, p_argv);
}

Global::Global(int p_argc, char* p_argv[], const Directory& p_root_dir) {
    singleton = this;
    file_system = std::make_unique<FileSystem>(p_root_dir);
    initialize(p_argc, p_argv);
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
    game_metadata = std::make_unique<GameMetadata>();
    parse_metadata(Path(singleton->file_system->get_resource_directory(), "metadata.json"));
    stdio_logging_manager = std::make_unique<LoggingManager<LogStream, std::ostream>>(std::cout);
}

}
