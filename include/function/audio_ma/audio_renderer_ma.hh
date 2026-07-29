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
#ifndef WBE_FILE_AUDIO_RENDERER_MA_HH
#define WBE_FILE_AUDIO_RENDERER_MA_HH

#include "function/audio/audio_renderer.hh"
#include <miniaudio.h>

namespace WhiteBirdEngine {

/**
 * @class AudioRendererMA
 * @brief MA audio renderer.
 *
 */
class AudioRendererMA final : public AudioRenderer {
public:
    AudioRendererMA();
    virtual ~AudioRendererMA() override;
    AudioRendererMA(const AudioRendererMA&) = delete;
    AudioRendererMA(AudioRendererMA&&) noexcept = delete;
    AudioRendererMA& operator=(const AudioRendererMA&) = delete;
    AudioRendererMA& operator=(AudioRendererMA&&) noexcept = delete;

private:
    ma_context context{};
    ma_device device{};
    ma_engine engine{};

    void init();
    void terminate();
};

} // namespace WhiteBirdEngine

#endif
