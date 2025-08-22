// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_GAME_CONFIG_HH__
#define __WBE_GAME_CONFIG_HH__

#include "core/parser/parser_yaml.hh"
#include "platform/file_system/path.hh"
#include "utils/interface/singleton.hh"
#include "utils/utils.hh"
#include <stdexcept>
namespace WhiteBirdEngine {

class GameConfig : public Singleton<GameConfig> {
public:

    GameConfig(const Path& p_config_file_path) {
        auto parser = ParserYAML();
        parser.parse(p_config_file_path);
        config_options.game_name = parser.get_value<std::string>("game_name");
        config_options.version_major = parser.get_value<uint32_t>("version_major");
        config_options.version_minor = parser.get_value<uint32_t>("version_minor");
        config_options.version_patch = parser.get_value<uint32_t>("version_patch");
        config_options.window_width = parser.get_value<uint32_t>("window_width");
        config_options.window_height = parser.get_value<uint32_t>("window_height");
        config_options.render_device_index = parser.get_value<int32_t>("render_device_index");
        config_options.render_api = get_render_api(parser.get_value<std::string>("render_api"));
    }
    ~GameConfig() {}

    const GameConfigOptions& get_config_options() const {
        return config_options;
    }

private:
    GameConfigOptions config_options;

    GameConfigOptions::RenderAPI get_render_api(const std::string& p_str) {
        if (p_str == "dummy") {
            return GameConfigOptions::RenderAPI::DUMMY;
        }
        if (p_str == "vulkan") {
            return GameConfigOptions::RenderAPI::VULKAN;
        }
        throw std::runtime_error("Unsupported rendering api: " + p_str + ".");
    }
};

}

#endif
