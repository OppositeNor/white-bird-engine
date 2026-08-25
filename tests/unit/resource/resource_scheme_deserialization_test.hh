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
        "geometry_path": "assets/meshes/test_submesh_01.bin",
        "geometry_sections": [
            {"slot": "position", "start": 0, "size": 36, "type": "vec3"},
            {"slot": "normal", "start": 36, "size": 36, "type": "vec3"},
            {"slot": "uv", "start": 72, "size": 24, "type": "vec2"},
            {"slot": "index", "start": 96, "size": 12, "type": "uint32"}
        ]
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
    ASSERT_EQ(scheme.geometry_path, "assets/meshes/test_submesh_01.bin");
    ASSERT_EQ(scheme.geometry_sections.size(), 4U);
    ASSERT_EQ(scheme.geometry_sections[0].slot, "position");
    ASSERT_EQ(scheme.geometry_sections[0].start, 0U);
    ASSERT_EQ(scheme.geometry_sections[0].size, 36U);
    ASSERT_EQ(scheme.geometry_sections[0].type, "vec3");
    ASSERT_EQ(scheme.geometry_sections[3].slot, "index");
    ASSERT_EQ(scheme.geometry_sections[3].start, 96U);
    ASSERT_EQ(scheme.geometry_sections[3].size, 12U);
    ASSERT_EQ(scheme.geometry_sections[3].type, "uint32");
}

TEST_F(WBEResourceSchemaDeserializationTest, RUSchemaSubmesh3DMinimal) {
    std::string json_buffer = R"(
    {
        "id": "test_submesh_minimal",
        "type": "submesh3d",
        "geometry_path": "assets/meshes/test_submesh_minimal.bin",
        "geometry_sections": []
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemaSubmesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_submesh_minimal");
    ASSERT_EQ(scheme.type, "submesh3d");
    ASSERT_EQ(scheme.geometry_path, "assets/meshes/test_submesh_minimal.bin");
    ASSERT_TRUE(scheme.geometry_sections.empty());
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
                "geometry_path": "assets/meshes/submesh_01.bin",
                "geometry_sections": []
            },
            {
                "id": "submesh_02",
                "type": "submesh3d",
                "geometry_path": "assets/meshes/submesh_02.bin",
                "geometry_sections": []
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
                    "type": "image",
                    "path": "images/normal.png",
                    "color_space": "rgb",
                    "channel_count": 3
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
    ASSERT_EQ(scheme.textures[1].texture->type, "image");
    ASSERT_EQ(scheme.textures[1].texture->path, "images/normal.png");
    ASSERT_EQ(scheme.textures[1].texture->color_space, "rgb");
    ASSERT_EQ(scheme.textures[1].texture->channel_count, 3U);
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

#endif
