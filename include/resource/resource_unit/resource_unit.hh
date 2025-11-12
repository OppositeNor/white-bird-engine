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
#ifndef __WBE_RESOURCE_UNIT_HH__
#define __WBE_RESOURCE_UNIT_HH__

#include "core/allocator/stack_allocator.hh"
#include "core/memory/reference_raw.hh"
#include "core/surface/3d_primitive.hh"
#include "utils/utils.hh"
#include <bitset>
#include <cstdint>
#include <type_traits>
namespace WhiteBirdEngine {

struct ResourceUnit {};

template <typename T>
constexpr bool is_resource_unit = std::is_base_of_v<ResourceUnit, std::remove_cvref_t<T>>;

template <typename T>
concept ResourceUnitConcept = is_resource_unit<T>;

template <typename T>
using RefRes = RefRaw<T, StackAllocator>;

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
    RefRes<char> audio_data;
};

/**
 * @class RUBinary
 * @brief Binary resource unit. contains a size (in bytes) and a MemID pointing to
 * the data chunk that stores the data of the binary data.
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 *
 */
struct RUBinary : public ResourceUnit {
    // The size of the binary data size (in bytes).
    size_t size;
    // The binary data.
    RefRes<char> data;
};

struct RUFont : public ResourceUnit {
    // TODO
};

/**
 * @class RUImage
 * @brief Image resource unit. Contails descriptions of an image
 * resource and a MemID to the resource of the image data.
 *
 * The image data should always be pushed as data before pushing this
 * description, and the release process should always be backwards
 * (first release the resource unit and then remove the data). It is
 * recommended to store the image data right before the image description
 * in a stack allocator.
 *
 */
struct RUImage : public ResourceUnit {
    enum ImageConfigBit {
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
    // The memory ID of the image data.
    RefRes<char> image_data;
};

/**
 * @class RUText
 * @brief Text resource unit. contains encoding type, size (in bytes, excluding
 * the NUL terminater) and a MemID pointing to the data chunk that stores the data
 * of the string (ends with a NUL terminater).
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 */
struct RUText : public ResourceUnit {
    // The encoding type.
    EncodeType encode_type = EncodeType::UTF8;
    // The size of the string (in bytes)
    size_t size;
    // The memid of the text data.
    RefRes<char> text_data;
};

struct RUVideo : public ResourceUnit {
    // TODO
};

struct RUMaterial : public ResourceUnit {
    // TODO
};

/**
 * @class RUSubmesh3D
 * @brief 3D submesh resource unit.
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 */
template <typename VertSlot>
struct RUSubmesh3D : public ResourceUnit {
    // The type of the vertex.
    using VertType = Vertex3D<VertSlot>;
    // A NUL terminating string referencing the material this submes uses.
    RefRes<char> material_key;
    // The vertices data.
    RefRes<VertType> vertices_data;
    // The indices data.
    RefRes<uint32_t> indices_data;
};

/**
 * @class RUMesh
 * @brief 3DD mesh resource unit.
 *
 */
struct RUMesh {
    // Number of submeshes.
    uint32_t submesh_count;
    // NUL terminated strings of keys of submeshes.
    RefRes<char> submesh_keys;
};

}

#endif
