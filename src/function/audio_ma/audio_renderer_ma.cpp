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
#include "function/audio_ma/audio_renderer_ma.hh"
#include "core/logging/log.hh"
#include "function/engine_function.hh"
#include "function/game_config/game_config.hh"
#include "function/game_config/game_config_options.hh"
#include <format>

namespace WhiteBirdEngine {

AudioRendererMA::AudioRendererMA() {
    init();
}

AudioRendererMA::~AudioRendererMA() {
    terminate();
}

void AudioRendererMA::init() {
    stdout_log(WBE_CHANNEL_DEBUG)->message("Initializing miniaudio audio renderer.");
    // Initialize ma context
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
        throw std::runtime_error("Failed to initialize miniaudio context");
    }

    // Get context devices
    ma_device_info* playback_infos;
    ma_uint32 playback_count;
    ma_device_info* capture_infos;
    ma_uint32 capture_count;
    if (ma_context_get_devices(&context, &playback_infos, &playback_count, &capture_infos, &capture_count) != MA_SUCCESS) {
        throw std::runtime_error("Failed to get audio device from context.");
    }

    // Get the prefered device from game config, if does not present, then use default instead.
    int32_t prefered_device_index = -1;
    const auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    for (uint32_t i = 0; i < playback_count; ++i) {
        if ((game_config.audio_device == "" && playback_infos[i].isDefault)
            || game_config.audio_device == playback_infos[i].name) {
            prefered_device_index = static_cast<int32_t>(i);
            break;
        }
    }

    // Initialize minaudio device
    ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
    if (prefered_device_index != -1) {
        stdout_log(WBE_CHANNEL_DEBUG)->message(std::format("Picked audio device: {}.", playback_infos[prefered_device_index].name));
        device_config.playback.pDeviceID = &playback_infos[prefered_device_index].id;
    }
    device_config.playback.format = ma_format_f32;
    device_config.playback.channels = 2;
    device_config.sampleRate = 44100;
    device_config.dataCallback = nullptr;
    device_config.pUserData = nullptr;
    if (ma_device_init(&context, &device_config, &device) != MA_SUCCESS) {
        throw std::runtime_error("Failed to initialize miniaudio device.");
    }

    // Initialize miniaudio engine
    ma_engine_config engine_config = ma_engine_config_init();
    engine_config.pDevice = &device;
    engine_config.pContext = &context;
    if (ma_engine_init(&engine_config, &engine) != MA_SUCCESS) {
        throw std::runtime_error("Failed to initialize miniaudio engine.");
    }
    stdout_log(WBE_CHANNEL_DEBUG)->message("Miniaudio audio renderer enabled.");
}

void AudioRendererMA::terminate() {
    ma_engine_uninit(&engine);
    ma_device_uninit(&device);
    ma_context_uninit(&context);
}

} // namespace WhiteBirdEngine
