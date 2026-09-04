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
#ifndef WBE_FILE_RESOURCE_SCHEME_DESERIALIZATION_TEST_HH
#define WBE_FILE_RESOURCE_SCHEME_DESERIALIZATION_TEST_HH

#include "core/parser/parser_json.hh"
#include "function/render/render_resource_unit.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <exception>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace WBE = WhiteBirdEngine;

class WBEResourceSchemaDeserializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
};

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaTextGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_text_01",
        "type": "text",
        "path": "texts/test_text_01.txt",
        "encode_type": "utf8"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaText scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_text_01");
    ASSERT_EQ(scheme.type, "text");
    ASSERT_EQ(scheme.path, "texts/test_text_01.txt");
    ASSERT_EQ(scheme.encode_type, "utf8");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaTextDefaults) {
    std::string json_buffer = R"(
    {
        "id": "test_text_02",
        "type": "text",
        "path": "texts/test_text_02.txt"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaText scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_text_02");
    ASSERT_EQ(scheme.type, "text");
    ASSERT_EQ(scheme.path, "texts/test_text_02.txt");
    ASSERT_EQ(scheme.encode_type, "utf8");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaAudioGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_audio_01",
        "type": "audio",
        "sample_rate": 44100,
        "channels": 2,
        "bit_depth": 16,
        "duration": 120.5,
        "frame_count": 5292000,
        "is_stream": false,
        "path": "audio/test_audio_01.wav"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaAudio scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_audio_01");
    ASSERT_EQ(scheme.type, "audio");
    ASSERT_EQ(scheme.sample_rate, 44100);
    ASSERT_EQ(scheme.channels, 2U);
    ASSERT_EQ(scheme.bit_depth, 16);
    ASSERT_FLOAT_EQ(scheme.duration, 120.5F);
    ASSERT_EQ(scheme.frame_count, 5292000);
    ASSERT_FALSE(scheme.is_stream);
    ASSERT_EQ(scheme.path, "audio/test_audio_01.wav");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaAudioDefaults) {
    std::string json_buffer = R"(
    {
        "id": "test_audio_02",
        "type": "audio",
        "is_stream": true,
        "path": "audio/test_audio_02.ogg"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaAudio scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_audio_02");
    ASSERT_EQ(scheme.type, "audio");
    ASSERT_EQ(scheme.sample_rate, -1);       // Default value
    ASSERT_EQ(scheme.channels, 1U);          // Default value
    ASSERT_EQ(scheme.bit_depth, -1);         // Default value
    ASSERT_FLOAT_EQ(scheme.duration, -1.0F); // Default value
    ASSERT_EQ(scheme.frame_count, -1);       // Default value
    ASSERT_TRUE(scheme.is_stream);
    ASSERT_EQ(scheme.path, "audio/test_audio_02.ogg");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaImageGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_image_01",
        "type": "image",
        "channel_count": 4,
        "color_space": "rgb",
        "is_hdr": false,
        "path": "images/test_image_01.png"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaImage scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_image_01");
    ASSERT_EQ(scheme.type, "image");
    ASSERT_EQ(scheme.channel_count, 4U);
    ASSERT_EQ(scheme.color_space, "rgb");
    ASSERT_FALSE(scheme.is_hdr);
    ASSERT_EQ(scheme.path, "images/test_image_01.png");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaImageHDR) {
    std::string json_buffer = R"(
    {
        "id": "test_image_hdr",
        "type": "image",
        "channel_count": 3,
        "color_space": "rgb",
        "is_hdr": true,
        "path": "images/test_image_hdr.hdr"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaImage scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_image_hdr");
    ASSERT_EQ(scheme.type, "image");
    ASSERT_EQ(scheme.channel_count, 3U);
    ASSERT_EQ(scheme.color_space, "rgb");
    ASSERT_TRUE(scheme.is_hdr);
    ASSERT_EQ(scheme.path, "images/test_image_hdr.hdr");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaImageDefaults) {
    std::string json_buffer = R"(
    {
        "id": "test_image_minimal",
        "type": "image",
        "path": "images/test_minimal.jpg"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaImage scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_image_minimal");
    ASSERT_EQ(scheme.type, "image");
    ASSERT_EQ(scheme.channel_count, 0);
    ASSERT_EQ(scheme.flip_h, false);
    ASSERT_EQ(scheme.flip_v, false);
    ASSERT_EQ(scheme.color_space, "srgb");
    ASSERT_FALSE(scheme.is_hdr);
    ASSERT_EQ(scheme.path, "images/test_minimal.jpg");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaTextureGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_texture_01",
        "type": "texture",
        "path": "textures/test_texture_01.ktx2"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaTexture scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_texture_01");
    ASSERT_EQ(scheme.type, "texture");
    ASSERT_EQ(scheme.path, "textures/test_texture_01.ktx2");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaBinaryGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_binary_01",
        "type": "binary",
        "path": "data/test_binary_01.bin"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaBinary scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_binary_01");
    ASSERT_EQ(scheme.type, "binary");
    ASSERT_EQ(scheme.path, "data/test_binary_01.bin");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaSubmesh3DGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_submesh_01",
        "type": "submesh3d",
        "material": {
            "id": "material_01",
            "type": "material"
        },
        "material_id": "mat_ref_01",
        "vertices": {
            "binary": {"binary_id": "test_submesh_01.geometry", "start": 0, "size": 96},
            "stride": 32,
            "attributes": [
                {"role": "position", "type": "vec3", "offset": 0},
                {"role": "normal", "type": "vec3", "offset": 12},
                {"role": "uv", "type": "vec2", "offset": 24}
            ]
        },
        "indices": {
            "binary": {"binary_id": "test_submesh_01.geometry", "start": 96, "size": 12}
        }
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaSubmesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_submesh_01");
    ASSERT_EQ(scheme.type, "submesh3d");
    ASSERT_TRUE(scheme.material.has_value());
    ASSERT_EQ(scheme.material.value().id, "material_01");
    ASSERT_EQ(scheme.material.value().type, "material");
    ASSERT_TRUE(scheme.material_id.has_value());
    ASSERT_EQ(scheme.material_id.value(), "mat_ref_01");
    ASSERT_EQ(scheme.vertices.binary.binary_id, "test_submesh_01.geometry");
    ASSERT_EQ(scheme.vertices.binary.start, 0U);
    ASSERT_EQ(scheme.vertices.binary.size, 96U);
    ASSERT_EQ(scheme.vertices.stride, 32U);
    ASSERT_EQ(scheme.vertices.attributes.size(), 3U);
    ASSERT_EQ(scheme.vertices.attributes[0].role, "position");
    ASSERT_EQ(scheme.vertices.attributes[0].type, "vec3");
    ASSERT_EQ(scheme.vertices.attributes[0].offset, 0U);
    ASSERT_EQ(scheme.vertices.attributes[2].role, "uv");
    ASSERT_EQ(scheme.vertices.attributes[2].type, "vec2");
    ASSERT_EQ(scheme.vertices.attributes[2].offset, 24U);
    ASSERT_EQ(scheme.indices.binary.binary_id, "test_submesh_01.geometry");
    ASSERT_EQ(scheme.indices.binary.start, 96U);
    ASSERT_EQ(scheme.indices.binary.size, 12U);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaSubmesh3DMinimal) {
    std::string json_buffer = R"(
    {
        "id": "test_submesh_minimal",
        "type": "submesh3d",
        "vertices": {
            "binary": {"binary_id": "test_submesh_minimal.geometry", "start": 0, "size": 12},
            "stride": 12,
            "attributes": [
                {"role": "position", "type": "vec3", "offset": 0}
            ]
        },
        "indices": {
            "binary": {"binary_id": "test_submesh_minimal.geometry", "start": 12, "size": 0}
        }
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaSubmesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_submesh_minimal");
    ASSERT_EQ(scheme.type, "submesh3d");
    ASSERT_EQ(scheme.vertices.binary.binary_id, "test_submesh_minimal.geometry");
    ASSERT_EQ(scheme.vertices.stride, 12U);
    ASSERT_EQ(scheme.vertices.attributes.size(), 1U);
    ASSERT_EQ(scheme.indices.binary.binary_id, "test_submesh_minimal.geometry");
    ASSERT_EQ(scheme.indices.binary.start, 12U);
    ASSERT_EQ(scheme.indices.binary.size, 0U);
    ASSERT_FALSE(scheme.material_id.has_value());
    ASSERT_FALSE(scheme.material.has_value());
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaMeshGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_mesh_01",
        "type": "mesh",
        "submeshes": [
            {
                "id": "submesh_01",
                "type": "submesh3d",
                "vertices": {
                    "binary": {"binary_id": "mesh.geometry", "start": 0, "size": 12},
                    "stride": 12,
                    "attributes": [{"role": "position", "type": "vec3", "offset": 0}]
                },
                "indices": {"binary": {"binary_id": "mesh.geometry", "start": 12, "size": 0}}
            },
            {
                "id": "submesh_02",
                "type": "submesh3d",
                "vertices": {
                    "binary": {"binary_id": "mesh.geometry", "start": 12, "size": 12},
                    "stride": 12,
                    "attributes": [{"role": "position", "type": "vec3", "offset": 0}]
                },
                "indices": {"binary": {"binary_id": "mesh.geometry", "start": 24, "size": 0}}
            }
        ],
        "submeshes_id": ["submesh_ref_01", "submesh_ref_02"]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaMesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_mesh_01");
    ASSERT_EQ(scheme.type, "mesh");
    ASSERT_EQ(scheme.submeshes.size(), 2U);
    ASSERT_EQ(scheme.submeshes[0].id, "submesh_01");
    ASSERT_EQ(scheme.submeshes[1].id, "submesh_02");
    ASSERT_EQ(scheme.submeshes_id.size(), 2U);
    ASSERT_EQ(scheme.submeshes_id[0], "submesh_ref_01");
    ASSERT_EQ(scheme.submeshes_id[1], "submesh_ref_02");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaMeshEmpty) {
    std::string json_buffer = R"(
    {
        "id": "test_mesh_empty",
        "type": "mesh"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaMesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_mesh_empty");
    ASSERT_EQ(scheme.type, "mesh");
    ASSERT_TRUE(scheme.submeshes.empty());
    ASSERT_TRUE(scheme.submeshes_id.empty());
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaStaticOpaqueSetGeneral) {
    std::string json_buffer = R"(
    {
        "id": "static_set_01",
        "type": "static_opaque_set",
        "submeshes": [
            {
                "material_id": "material_01",
                "vertices": {
                    "binary": {"binary_id": "static_set_01.geometry", "start": 0, "size": 36},
                    "stride": 12,
                    "attributes": [{"role": "position", "type": "vec3", "offset": 0}]
                },
                "indices": {"binary": {"binary_id": "static_set_01.geometry", "start": 36, "size": 12}},
                "first_instance": 1,
                "instance_count": 2
            }
        ],
        "instances": [
            {"global_transform": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]},
            {"global_transform": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 3, 4, 1]}
        ]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaStaticOpaqueSet scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "static_set_01");
    ASSERT_EQ(scheme.type, "static_opaque_set");
    ASSERT_EQ(scheme.submeshes.size(), 1U);
    ASSERT_TRUE(scheme.submeshes[0].material_id.has_value());
    ASSERT_EQ(scheme.submeshes[0].material_id.value(), "material_01");
    ASSERT_EQ(scheme.submeshes[0].vertices.binary.binary_id, "static_set_01.geometry");
    ASSERT_EQ(scheme.submeshes[0].vertices.stride, 12U);
    ASSERT_EQ(scheme.submeshes[0].vertices.attributes[0].role, "position");
    ASSERT_EQ(scheme.submeshes[0].indices.binary.start, 36U);
    ASSERT_EQ(scheme.submeshes[0].first_instance, 1U);
    ASSERT_EQ(scheme.submeshes[0].instance_count, 2U);
    ASSERT_EQ(scheme.instances.size(), 2U);
    ASSERT_EQ(scheme.instances[1].global_transform[3][0], 2.0F);
    ASSERT_EQ(scheme.instances[1].global_transform[3][1], 3.0F);
    ASSERT_EQ(scheme.instances[1].global_transform[3][2], 4.0F);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaStaticMaskedSetEmpty) {
    std::string json_buffer = R"(
    {
        "id": "static_masked_set_01",
        "type": "static_masked_set",
        "submeshes": [],
        "instances": []
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaStaticMaskedSet scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "static_masked_set_01");
    ASSERT_EQ(scheme.type, "static_masked_set");
    ASSERT_TRUE(scheme.submeshes.empty());
    ASSERT_TRUE(scheme.instances.empty());
}

// Basic tests for TODO schemes to ensure they can be deserialized
TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaFont) {
    // TODO: Expand this test once RUSchemaFont is fully implemented
    std::string json_buffer = R"(
    {
        "id": "test_font_01",
        "type": "font"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaFont scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_font_01");
    ASSERT_EQ(scheme.type, "font");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaVideo) {
    // TODO: Expand this test once RUSchemaVideo is fully implemented
    std::string json_buffer = R"(
    {
        "id": "test_video_01",
        "type": "video"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaVideo scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_video_01");
    ASSERT_EQ(scheme.type, "video");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaMaterial) {
    std::string json_buffer = R"(
    {
        "id": "test_material_01",
        "type": "material",
        "graphics_pipeline_ids": ["main_pipeline", "shadow_pipeline"],
        "textures": [
            {
                "texture_role": "albedo",
                "texture_id": "albedo_texture"
            },
            {
                "texture_role": "normal",
                "texture": {
                    "id": "normal_texture",
                    "type": "texture",
                    "path": "textures/normal.ktx2"
                }
            }
        ]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaMaterial scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_material_01");
    ASSERT_EQ(scheme.type, "material");
    ASSERT_EQ(scheme.graphics_pipeline_ids.size(), 2U);
    ASSERT_EQ(scheme.graphics_pipeline_ids[0], "main_pipeline");
    ASSERT_EQ(scheme.graphics_pipeline_ids[1], "shadow_pipeline");
    ASSERT_EQ(scheme.textures.size(), 2U);
    ASSERT_EQ(scheme.textures[0].texture_role, "albedo");
    ASSERT_TRUE(scheme.textures[0].texture_id.has_value());
    ASSERT_EQ(*scheme.textures[0].texture_id, "albedo_texture");
    ASSERT_FALSE(scheme.textures[0].texture.has_value());
    ASSERT_EQ(scheme.textures[1].texture_role, "normal");
    ASSERT_FALSE(scheme.textures[1].texture_id.has_value());
    ASSERT_TRUE(scheme.textures[1].texture.has_value());
    ASSERT_EQ(scheme.textures[1].texture->id, "normal_texture");
    ASSERT_EQ(scheme.textures[1].texture->type, "texture");
    ASSERT_EQ(scheme.textures[1].texture->path, "textures/normal.ktx2");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaBaseMissingType) {
    std::string json_buffer = R"(
    {
        "id": "test_resource",
        "path": "texts/test.txt"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaText scheme;
    // Should throw because type is required in base RUSchema
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaBaseMissingBothIdAndType) {
    std::string json_buffer = R"(
    {
        "path": "texts/test.txt"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaText scheme;
    // Should throw because both id and type are required in base RUSchema
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaImageMissingType) {
    std::string json_buffer = R"(
    {
        "id": "test_image",
        "channel_count": 3,
        "color_space": "rgb",
        "is_hdr": false,
        "path": "images/test.jpg"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaImage scheme;
    // Should throw because type is required in base RUSchema
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaTextMissingRequiredField) {
    std::string json_buffer = R"(
    {
        "id": "test_text_error",
        "type": "text"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaText scheme;
    // Should throw because path is required
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaAudioMissingRequiredFields) {
    std::string json_buffer = R"(
    {
        "id": "test_audio_error",
        "type": "audio"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaAudio scheme;
    // Should throw because is_stream and path are required
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaImageMissingRequiredFields) {
    std::string json_buffer = R"(
    {
        "id": "test_image_error",
        "type": "image"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaImage scheme;
    // Should throw because channel_count, color_space, is_hdr, and path are required
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaTextureMissingPath) {
    std::string json_buffer = R"(
    {
        "id": "test_texture_error",
        "type": "texture"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaTexture scheme;
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

#endif
