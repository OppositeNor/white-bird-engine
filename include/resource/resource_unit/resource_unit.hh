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
#include "core/surface/3d_primitive.hh"
#include "utils/utils.hh"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
namespace WhiteBirdEngine {

struct ResourceUnit {};

template <typename T>
constexpr bool IS_RESOURCE_UNIT = std::is_base_of_v<ResourceUnit, std::remove_cvref_t<T>>;

template <typename T>
concept ResourceUnitConcept = IS_RESOURCE_UNIT<T>;

template <typename T>
using RefRes = RefRaw<T, IAllocator>;

/**
 * @class ResourceScheme
 * @brief The scheme of a resource unit.
 *
 */
struct WBE_META(WBE_SERIALIZABLE) RUScheme : public Serializable {
    WBE_DECL_SERIALIZABLE(RUScheme)
    // The id of the resource. Default to empty string, which is an ananymous resource.
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
 * @class RUSchemeShader
 * @brief The scheme of RUShader.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeShader : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeShader)
    // The path to the shader file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string path;
    // The path to the shader's metadata file.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::string metadata;
};

/**
 * @class RUSchemePipeline
 * @brief The scheme of RUPipeline.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemePipeline : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemePipeline)
    // The resource ID of the vertex shader.
    WBE_META(WBE_REFLECT)
    std::string vertex_stage_id;
    // The vertex shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader vertex_stage;
    // The resource ID of the tessellation shader.
    WBE_META(WBE_REFLECT)
    std::string tessellation_stage_id;
    // The tessellation shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader tessellation_stage;
    // The resource ID of the geometry shader.
    WBE_META(WBE_REFLECT)
    std::string geometry_stage_id;
    // The geometry shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader geometry_stage;
    // The resource ID of the fragment shader.
    WBE_META(WBE_REFLECT)
    std::string fragment_stage_id;
    // The fragment shader of the pipeline.
    WBE_META(WBE_REFLECT)
    RUSchemeShader fragment_stage;
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
 * @class RUSchemeMaterial
 * @brief The scheme of RUMaterial.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeMaterial : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeMaterial)
    // TODO
};

/**
 * @class RUSchemeSubmesh3D
 * @brief The scheme of RUSubmesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeSubmesh3D : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeSubmesh3D)
    // The material if it could be populated in place.
    WBE_META(WBE_REFLECT)
    RUSchemeMaterial material;
    // A string referencing unpopulated material.
    WBE_META(WBE_REFLECT)
    std::string material_id;
    // The vertices data.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<Vertex3DTextured> vertices_data;
    // The indices data.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<uint32_t> indices_data;
};

/**
 * @class RUSchemeMesh3D
 * @brief The scheme of RUMesh3D.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeMesh3D : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeMesh3D)
    // Submeshes that could be populated in place.
    WBE_META(WBE_REFLECT)
    std::vector<RUSchemeSubmesh3D> submeshes;
    // Submesh ids referencing unpopulated submeshes.
    WBE_META(WBE_REFLECT)
    std::vector<std::string> submeshes_id;
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
    // The size of the binary data.
    size_t size;
};

/**
 * @class RUShader
 * @brief Shader resource unit. contains a size (in bytes) and a MemID pointing to
 * the data chunk that stores the data of the binary data.
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 *
 */
struct RUShader : public ResourceUnit {
    // The binary data.
    RefRes<char> data;
    // The size of the binary data.
    size_t size;
    // TODO: Shader metadata.
};

/**
 * @class RUPipeline
 * @brief Pipeline.
 *
 */
struct RUPipeline : public ResourceUnit {
    // The vertex shader.
    RefRes<RUShader> vertex_stage;
    // The tessellation shader.
    RefRes<RUShader> tessellation_stage;
    // The geometry shader.
    RefRes<RUShader> geometry_stage;
    // The fragment shader.
    RefRes<RUShader> fragment_stage;
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
 * @class RUText
 * @brief Text resource unit. contains encoding type, size (in bytes, including
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

/**
 * @class RUMaterial
 * @brief Material resource unit.
 */
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
struct RUSubmesh3D : public ResourceUnit {
    // A reference to the material.
    RefRes<RUMaterial> material;
    // The total number of triangles.
    size_t triangles_count;
    // The triangles instances.
    RefRes<Triangle3DIndxTextured> triangles;
};

/**
 * @class RUMesh3D
 * @brief 3D mesh resource unit.
 */
struct RUMesh3D : public ResourceUnit {
    // Number of submeshes.
    size_t submesh_count;
    // The submeshes.
    RefRes<RefRes<RUSubmesh3D>> submeshes;
};

} // namespace WhiteBirdEngine

#endif
