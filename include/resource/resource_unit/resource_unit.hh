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
#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"
#include "core/surface/3d_primitive.hh"
#include "global/stl_allocator.hh"
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
 * @class ResourceScheme
 * @brief The scheme of a resource unit.
 *
 */
struct WBE_META(WBE_SERIALIZABLE) ResourceUnitScheme : public Serializable {
    WBE_DECL_SERIALIZABLE(ResourceUnitScheme)
    // The id of the resource.
    string<> id;
    // The type of the resource.
    string<> type;
};

/**
 * @class RUSchemeAudio
 * @brief The scheme of RUAudio.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeAudio final : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeAudio)
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
    // Is the audio loaded in stream.
    bool is_stream;
    // The path to the audio file.
    string<> path;
};

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
    /**
     * @brief Configurations to this resource unit.
     */
    enum AudioConfig {
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
 * @class RUSchemeBinary
 * @brief The scheme of RUBinary.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeBinary : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeBinary)
    // The path to the binary file.
    string<> path;
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
    // The binary data.
    RefRes<char> data;
};

/**
 * @class RUSchemeFont
 * @brief The scheme of RUFont.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeFont : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeFont)
    // TODO
};

/**
 * @class RUFont
 * @brief Font resource unit.
 */
struct RUFont : public ResourceUnit {
    // TODO
};

/**
 * @class RUSchemeImage
 * @brief The scheme of RUImage.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeImage : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeImage)
    // The width of the image (in pixels).
    uint32_t width;
    // The height of the image (in pixels).
    uint32_t height;
    // The channel count of the image.
    uint32_t channel_count;
    // The color space of the image.
    string<> color_space;
    // Image configurations.
    bool is_hdr;
    // The path to the image file.
    string<> path;
};

/**
 * @class RUImage
 * @brief Image resource unit. Contains descriptions of an image
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
    /**
     * @brief Configurations to this resource unit.
     */
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
 * @class RUSchemeText
 * @brief The scheme of RUText.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeText : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeText)
    // The path to the text file.
    string<> path;
    // The encoding type.
    string<> encode_type;
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
    // The text data.
    RefRes<char> text_data;
};

/**
 * @class RUSchemeVideo
 * @brief The scheme of RUVideo.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeVideo : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeVideo)
    // TODO
};

/**
 * @class RUVideo
 * @brief Video resource unit.
 */
struct RUVideo : public ResourceUnit {
    // TODO
};

/**
 * @class RUSchemeMaterial
 * @brief The scheme of RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeMaterial : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeMaterial)
    // TODO
};

/**
 * @class RUMaterial
 * @brief Material resource unit.
 */
struct RUMaterial : public ResourceUnit {
    // TODO
};

/**
 * @class RUSchemeSubmesh3D
 * @brief The scheme of RUSubmesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeSubmesh3D : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeSubmesh3D)
    // The material if it could be populated in place.
    RUSchemeMaterial material;
    // A string referencing unpopulated material.
    string<> material_id;
    // The vertices data.
    vector<Vertex3DTextured> vertices_data;
    // The indices data.
    vector<uint32_t> indices_data;
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
struct RUSubmesh3D : public ResourceUnit {
    // A reference to the material.
    RefRes<RUMaterial> material;
    RefRes<Triangle3DIndxTextured> triangles;
};

/**
 * @class RUSchemeMesh
 * @brief The scheme of RUMesh.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeMesh : public ResourceUnitScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeMesh)
    // Submeshes that could be populated in place.
    vector<RUSchemeSubmesh3D> submeshes;
    // Submesh ids referencing unpopulated submeshes.
    vector<string<>> submeshes_id;
};

/**
 * @class RUMesh
 * @brief 3D mesh resource unit.
 */
struct RUMesh : public ResourceUnit {
    // The submeshes.
    RefRes<RefRes<RUSubmesh3D>> submeshes;
};

}

#endif
