#include "core/global/global.h"
#include "core/global/game_metadata.h"
#include "core/parser/parser_json.h"
#include "platform/file_system/file_system.h"
#include <memory>

namespace WhiteBirdEngine {

void Global::init(int p_argc, char* p_argv[]) {
    if (singleton != nullptr) {
        throw std::runtime_error("Global instance initialized multiple times.");
    }
    singleton = new Global(p_argc, p_argv);
    singleton->file_system = std::make_unique<FileSystem>();
    singleton->game_metadata = std::make_unique<GameMetadata>();
    parse_metadata(Path(singleton->file_system->get_resource_directory(), "metadata.json"));
}

void Global::terminate() {
    delete singleton;
}

Global::Global(int p_argc, char* p_argv[])
    : Singleton<Global>() {
    // TODO
}

Global::~Global() {
    // TODO
}

Global* Global::get_singleton() {
    return nullptr; // TODO
}

void Global::parse_metadata(const Path& p_metadata_config_path) {
    ParserJSON parser;
    parser.parse(p_metadata_config_path);
    auto channels_data = parser.get_value<JSONData>("channels");
    auto channel_keys = channels_data.get_all_keys();
    for (auto& key : channel_keys) {
        singleton->game_metadata->get_channel_metadata().add_pair(key, channels_data.get_value<ChannelID>(key));
    }
}

}
