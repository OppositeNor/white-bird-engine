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
#ifndef WBE_FILE_RESOURCE_UNIT_HH
#define WBE_FILE_RESOURCE_UNIT_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_raw.hh"
#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"
#include "utils/utils.hh"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
namespace WhiteBirdEngine {

/**
 * @class ResourceUnit
 * @brief Base type for manifest-loaded resource units.
 *
 * Resource units are stored inside resource chunks and may now be constructed
 * and destructed explicitly by the chunk. They are no longer required to be
 * trivially destructible.
 */
struct ResourceUnit {
    /**
     * @brief Stable key used to find this resource in its owning chunk.
     *
     * The string memory is owned by the resource chunk that stores this unit.
     * Anonymous resources leave this as nullptr.
     */
    const char* resource_key = nullptr;
};

template <typename T>
constexpr bool IS_RESOURCE_UNIT = std::is_base_of_v<ResourceUnit, std::remove_cvref_t<T>>;

template <typename T>
concept ResourceUnitConcept = IS_RESOURCE_UNIT<T>;

template <typename T>
using RefRes = RefRaw<T, IAllocator, false>;

/**
 * @class ResourceScheme
 * @brief The scheme of a resource unit.
 *
 */
struct WBE_META(WBE_SERIALIZABLE) RUScheme : public Serializable {
    WBE_DECL_SERIALIZABLE(RUScheme)
    // The id of the resource. Default to empty string, which is an ananymous
    // resource.
    WBE_META(WBE_REFLECT)
    std::string id;
    // The type of the resource.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string type;
};

/**
 * @class RUSchemeAudio
 * @brief The scheme of RUAudio.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeAudio final : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeAudio)
    // Sample rate of the audio. Could be read from file.
    WBE_META(WBE_REFLECT)
    int32_t sample_rate = -1;
    // The number of channels of the audio (typically 1 or 2). Could be read
    // from file.
    WBE_META(WBE_REFLECT)
    uint32_t channels = 1;
    // The bit depth per sample. Could be read from file.
    WBE_META(WBE_REFLECT)
    int32_t bit_depth = -1;
    // The duration of the audio. Could be read from file.
    WBE_META(WBE_REFLECT)
    float duration = -1.0F;
    // The number of frames of the audio. Could be read from file.
    WBE_META(WBE_REFLECT)
    int32_t frame_count = -1;
    // Is the audio loaded in stream.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    bool is_stream;
    // The path to the audio file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
};

/**
 * @class RUSchemeBinary
 * @brief The scheme of RUBinary.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeBinary : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeBinary)
    // The path to the binary file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
};

/**
 * @class RUSchemeFont
 * @brief The scheme of RUFont.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeFont : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeFont)
    // TODO
};

/**
 * @class RUSchemeImage
 * @brief The scheme of RUImage.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeImage : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeImage)
    // The channel count of the image. Read from file if set to 0.
    WBE_META(WBE_REFLECT)
    uint32_t channel_count = 0;
    // Image configurations.
    WBE_META(WBE_REFLECT)
    bool is_hdr = false;
    // Flip the image horizontally.
    WBE_META(WBE_REFLECT)
    bool flip_h = false;
    // Flip the image vertically.
    WBE_META(WBE_REFLECT)
    bool flip_v = false;
    // The color space of the image.
    WBE_META(WBE_REFLECT)
    std::string color_space = "srgb";
    // The path to the image file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
};

/**
 * @class RUSchemeText
 * @brief The scheme of RUText.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeText : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeText)
    // The path to the text file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
    // The encoding type. UTF8 by default.
    WBE_META(WBE_REFLECT)
    std::string encode_type = "utf8";
};

/**
 * @class RUSchemeVideo
 * @brief The scheme of RUVideo.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeVideo : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeVideo)
    // TODO
};

/**
 * @class RUAudio
 * @brief Audio data resource. Contains description of an audio,
 * and a MemID referencing the resource of the audio data.
 *
 * When this unit references chunk-owned payload data, push that payload before
 * emplacing the resource unit so addresses stay stable inside the stack
 * allocator. Chunk teardown now runs any registered resource-unit destructors
 * before releasing the underlying stack storage.
 */
struct RUAudio : public ResourceUnit {
    /**
     * @brief Configurations to this resource unit.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
    enum AudioConfig {
        // Should the audio be streamed. If true, then audio data will be a
        // stb_vorbis object.
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
    RefRes<char> audio_data;
};

/**
 * @class RUBinary
 * @brief Binary resource unit. contains a size (in bytes) and a MemID pointing
 * to the data chunk that stores the data of the binary data.
 *
 * Push the payload data before emplacing this unit when it references
 * chunk-owned memory. Chunk teardown runs registered resource-unit destructors
 * before releasing the allocator storage that backs both the unit and its
 * payload.
 *
 */
struct RUBinary : public ResourceUnit {
    // The binary data.
    RefRes<char> data;
    // The size of the binary data.
    size_t size;
};

/**
 * @class RUFont
 * @brief Font resource unit.
 */
struct RUFont : public ResourceUnit {
    // TODO
};

/**
 * @class RUImage
 * @brief Image resource unit. Contains descriptions of an image
 * resource and a MemID to the resource of the image data.
 *
 * When this unit references chunk-owned image payload data, push that payload
 * before emplacing the unit so the stored references remain valid. Chunk
 * teardown runs registered resource-unit destructors before releasing the
 * underlying stack storage.
 *
 */
struct RUImage : public ResourceUnit {
    /**
     * @brief Configurations to this resource unit.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
    enum ImageConfig {
        // Is the image an HDR image (32 bit instead of 8 bit)
        IS_HDR,
        // To track the total number of bits.
        TOTAL_CONFIG_BIT
    };
    // The width of the image (in pixels).
    uint32_t width;
    // The height of the image (in pixels).
    uint32_t height;
    // The channel count of the image.
    uint32_t channel_count;
    // The color space of the image.
    ColorSpace color_space;
    // Image configurations.
    std::bitset<TOTAL_CONFIG_BIT> config;
    // The image data.
    RefRes<char> image_data;
};

/**
 * @class RUText
 * @brief Text resource unit. contains encoding type, size (in bytes, including
 * the NUL terminater) and a MemID pointing to the data chunk that stores the
 * data of the string (ends with a NUL terminater).
 *
 * Push the string payload before emplacing this unit when it references
 * chunk-owned memory. Chunk teardown runs registered resource-unit destructors
 * before releasing the allocator storage that backs both the unit and its
 * payload.
 */
struct RUText : public ResourceUnit {
    // The encoding type.
    EncodeType encode_type = EncodeType::UTF8;
    // The text data.
    RefRes<char> text_data;
    // The size of the text data, including the NUL terminator.
    size_t size;
};

/**
 * @class RUVideo
 * @brief Video resource unit.
 */
struct RUVideo : public ResourceUnit {
    // TODO
};

} // namespace WhiteBirdEngine

#endif
