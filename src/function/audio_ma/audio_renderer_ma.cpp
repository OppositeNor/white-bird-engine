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
#include "function/engine_function.hh"
#include "function/game_config/game_config.hh"
#include "function/game_config/game_config_options.hh"

namespace WhiteBirdEngine {

AudioRendererMA::AudioRendererMA() {
    init();
}

AudioRendererMA::~AudioRendererMA() {
    terminate();
}

void AudioRendererMA::init() {
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
    for (int32_t i = 0; i < playback_count; ++i) {
        if ((game_config.audio_device == "" && playback_infos[i].isDefault)
            || game_config.audio_device == playback_infos[i].name) {
            prefered_device_index = i;
            break;
        }
    }
    
    // Initialize minaudio device
    ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.pDeviceID = &(playback_infos[prefered_device_index].id);
    // TODO
}

void AudioRendererMA::terminate() {
    ma_context_uninit(&context);
}

} // namespace WhiteBirdEngine
