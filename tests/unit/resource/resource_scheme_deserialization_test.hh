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

#include <exception>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "core/parser/parser_json.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "resource/resource_unit/resource_unit.hh"

namespace WBE = WhiteBirdEngine;

class WBEResourceSchemeDeserializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
};

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeTextGeneral) {
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
    WBE::RUSchemeText scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_text_01");
    ASSERT_EQ(scheme.type, "text");
    ASSERT_EQ(scheme.path, "texts/test_text_01.txt");
    ASSERT_EQ(scheme.encode_type, "utf8");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeTextDefaults) {
    std::string json_buffer = R"(
    {
        "id": "test_text_02",
        "type": "text",
        "path": "texts/test_text_02.txt"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeText scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_text_02");
    ASSERT_EQ(scheme.type, "text");
    ASSERT_EQ(scheme.path, "texts/test_text_02.txt");
    ASSERT_EQ(scheme.encode_type, "utf8"); // Default value
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeAudioGeneral) {
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
    WBE::RUSchemeAudio scheme;
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

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeAudioDefaults) {
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
    WBE::RUSchemeAudio scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_audio_02");
    ASSERT_EQ(scheme.type, "audio");
    ASSERT_EQ(scheme.sample_rate, -1); // Default value
    ASSERT_EQ(scheme.channels, 1U); // Default value
    ASSERT_EQ(scheme.bit_depth, -1); // Default value
    ASSERT_FLOAT_EQ(scheme.duration, -1.0F); // Default value
    ASSERT_EQ(scheme.frame_count, -1); // Default value
    ASSERT_TRUE(scheme.is_stream);
    ASSERT_EQ(scheme.path, "audio/test_audio_02.ogg");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeImageGeneral) {
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
    WBE::RUSchemeImage scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_image_01");
    ASSERT_EQ(scheme.type, "image");
    ASSERT_EQ(scheme.channel_count, 4U);
    ASSERT_EQ(scheme.color_space, "rgb");
    ASSERT_FALSE(scheme.is_hdr);
    ASSERT_EQ(scheme.path, "images/test_image_01.png");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeImageHDR) {
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
    WBE::RUSchemeImage scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_image_hdr");
    ASSERT_EQ(scheme.type, "image");
    ASSERT_EQ(scheme.channel_count, 3U);
    ASSERT_EQ(scheme.color_space, "rgb");
    ASSERT_TRUE(scheme.is_hdr);
    ASSERT_EQ(scheme.path, "images/test_image_hdr.hdr");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeImageDefaults) {
    std::string json_buffer = R"(
    {
        "id": "test_image_minimal",
        "type": "image",
        "path": "images/test_minimal.jpg"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeImage scheme;
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

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeBinaryGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_binary_01",
        "type": "binary",
        "path": "data/test_binary_01.bin"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeBinary scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_binary_01");
    ASSERT_EQ(scheme.type, "binary");
    ASSERT_EQ(scheme.path, "data/test_binary_01.bin");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeSubmesh3DGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_submesh_01",
        "type": "submesh3d",
        "material": {
            "id": "material_01",
            "type": "material"
        },
        "material_id": "mat_ref_01",
        "vertices_data": [
            {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "normal": {"x": 0.0, "y": 1.0, "z": 0.0},
                "uv": {"x": 0.0, "y": 0.0}
            },
            {
                "position": {"x": 1.0, "y": 0.0, "z": 0.0},
                "normal": {"x": 0.0, "y": 1.0, "z": 0.0},
                "uv": {"x": 1.0, "y": 0.0}
            },
            {
                "position": {"x": 0.0, "y": 0.0, "z": 1.0},
                "normal": {"x": 0.0, "y": 1.0, "z": 0.0},
                "uv": {"x": 0.0, "y": 1.0}
            }
        ],
        "indices_data": [0, 1, 2]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeSubmesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_submesh_01");
    ASSERT_EQ(scheme.type, "submesh3d");
    ASSERT_EQ(scheme.material.id, "material_01");
    ASSERT_EQ(scheme.material.type, "material");
    ASSERT_EQ(scheme.material_id, "mat_ref_01");
    ASSERT_EQ(scheme.vertices_data.size(), 3U);
    ASSERT_EQ(scheme.indices_data.size(), 3U);
    ASSERT_EQ(scheme.indices_data[0], 0U);
    ASSERT_EQ(scheme.indices_data[1], 1U);
    ASSERT_EQ(scheme.indices_data[2], 2U);
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeSubmesh3DMinimal) {
    std::string json_buffer = R"(
    {
        "id": "test_submesh_minimal",
        "type": "submesh3d",
        "vertices_data": [],
        "indices_data": []
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeSubmesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_submesh_minimal");
    ASSERT_EQ(scheme.type, "submesh3d");
    ASSERT_TRUE(scheme.vertices_data.empty());
    ASSERT_TRUE(scheme.indices_data.empty());
    ASSERT_TRUE(scheme.material_id.empty());
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeMeshGeneral) {
    std::string json_buffer = R"(
    {
        "id": "test_mesh_01",
        "type": "mesh",
        "submeshes": [
            {
                "id": "submesh_01",
                "type": "submesh3d",
                "vertices_data": [],
                "indices_data": []
            },
            {
                "id": "submesh_02",
                "type": "submesh3d",
                "vertices_data": [],
                "indices_data": []
            }
        ],
        "submeshes_id": ["submesh_ref_01", "submesh_ref_02"]
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeMesh3D scheme;
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

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeMeshEmpty) {
    std::string json_buffer = R"(
    {
        "id": "test_mesh_empty",
        "type": "mesh"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeMesh3D scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_mesh_empty");
    ASSERT_EQ(scheme.type, "mesh");
    ASSERT_TRUE(scheme.submeshes.empty());
    ASSERT_TRUE(scheme.submeshes_id.empty());
}

// Basic tests for TODO schemes to ensure they can be deserialized
TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeFont) {
    // TODO: Expand this test once RUSchemeFont is fully implemented
    std::string json_buffer = R"(
    {
        "id": "test_font_01",
        "type": "font"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeFont scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_font_01");
    ASSERT_EQ(scheme.type, "font");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeVideo) {
    // TODO: Expand this test once RUSchemeVideo is fully implemented
    std::string json_buffer = R"(
    {
        "id": "test_video_01",
        "type": "video"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeVideo scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_video_01");
    ASSERT_EQ(scheme.type, "video");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeMaterial) {
    // TODO: Expand this test once RUSchemeMaterial is fully implemented
    std::string json_buffer = R"(
    {
        "id": "test_material_01",
        "type": "material"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeMaterial scheme;
    scheme.deserialize(parser.get_data());
    ASSERT_EQ(scheme.id, "test_material_01");
    ASSERT_EQ(scheme.type, "material");
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeBaseMissingType) {
    std::string json_buffer = R"(
    {
        "id": "test_resource",
        "path": "texts/test.txt"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeText scheme;
    // Should throw because type is required in base RUScheme
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeBaseMissingBothIdAndType) {
    std::string json_buffer = R"(
    {
        "path": "texts/test.txt"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeText scheme;
    // Should throw because both id and type are required in base RUScheme
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeImageMissingType) {
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
    WBE::RUSchemeImage scheme;
    // Should throw because type is required in base RUScheme
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeTextMissingRequiredField) {
    std::string json_buffer = R"(
    {
        "id": "test_text_error",
        "type": "text"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeText scheme;
    // Should throw because path is required
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeAudioMissingRequiredFields) {
    std::string json_buffer = R"(
    {
        "id": "test_audio_error",
        "type": "audio"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeAudio scheme;
    // Should throw because is_stream and path are required
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEResourceSchemeDeserializationTest, RUSchemeImageMissingRequiredFields) {
    std::string json_buffer = R"(
    {
        "id": "test_image_error",
        "type": "image"
    }
    )";
    WBE::ParserJSON parser;
    parser.parse_from_buffer(json_buffer);
    WBE::RUSchemeImage scheme;
    // Should throw because channel_count, color_space, is_hdr, and path are required
    EXPECT_THROW(scheme.deserialize(parser.get_data()), std::exception);
}


#endif
