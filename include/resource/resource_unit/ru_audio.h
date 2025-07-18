#ifndef __WBE_RU_AUDIO_EXT__
#define __WBE_RU_AUDIO_EXT__

#include "core/allocator/allocator.h"
#include "resource_unit.h"
#include <bitset>
#include <cstdint>
namespace WhiteBirdEngine {

/**
 * @class RUAudio
 * @brief Audio data resource. Contains description of an audio,
 * and a MemID referencing the resource of the audio data.
 *
 * The audio data should always be pushed as data before pushing this
 * description, and the release process should always be backwards
 * (first release the resource unit and then remove the data). It is
 * recommended to store the audio data right before the audio description
 * in a stack allocator.
 */
struct RUAudio : public ResourceUnit {
    enum AudioConfigBit {
        // Should the audio be streamed. If true, then audio data will be a stb_vorbis object.
        IS_STREAM,
        // To track the total number of bits.
        TOTAL_CONFIG_BIT
    };
    // Sample rate of the audio.
    uint32_t sample_rate;
    // The number of channels of the audio (typically 1 or 2).
    uint32_t channels;
    // The bit depth per sample.
    uint32_t bit_depth;
    // The duration of the audio.
    float duration;
    // The number of frames of the audio.
    uint32_t frame_count;
    // Configurations of the audio.
    std::bitset<TOTAL_CONFIG_BIT> config;
    // The data of the audio.
    MemID audio_data;
};

}

#endif
