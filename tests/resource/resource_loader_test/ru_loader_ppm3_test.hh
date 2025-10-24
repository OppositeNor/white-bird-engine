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
#ifndef __WBE_RU_LOADER_PPM3_TEST_HH__
#define __WBE_RU_LOADER_PPM3_TEST_HH__
#include "core/allocator/allocator.hh"
#include "core/parser/parser_json.hh"
#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/ru_loader_ppm3.hh"
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>

namespace WBE = WhiteBirdEngine;

class RULoaderPPM3Test : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
        WBE::ParserJSON parser;
        parser.parse_from_buffer(test_ppm3_json);
        test_ppm3_json_data = parser.get_data();
    }

    std::unique_ptr<WBE::Global> global;

    std::string test_ppm3_json =
    R"({)"
    R"("path" : "images/test_ppm3.ppm",)"
    R"("color_space" : "rgb",)"
    R"("flip_h" : false,)"
    R"("flip_v" : false)"
    R"(})";
    WBE::JSONData test_ppm3_json_data;
};

class MockResourceChunk : public WBE::ResourceChunk {
public:
    MockResourceChunk() {
        ss << "Construct()" << std::endl;
    }

    virtual ~MockResourceChunk() override {
        ss << "Destruct()" << std::endl;
    }

    mutable std::stringstream ss;
    std::vector<std::unique_ptr<char[]>> pushed_res;
    std::vector<std::unique_ptr<char[]>> pushed_data;

    virtual WBE::RefRes<char> get_resource(const std::string& p_key) const override {
        ss << "const get_resource(" << p_key << ")" << std::endl;
        return WBE::MEM_NULL;
    }

    virtual size_t get_size() const override {
        ss << "get_size()" << std::endl;
        return 0;
    }

    virtual WBE::RefRes<char> push_data(size_t p_res_size, void* p_res) override {
        ss << "push_data(" << p_res_size << ", pushed_data)" << std::endl;
        std::unique_ptr<char[]> res_data = std::make_unique<char[]>(p_res_size);
        memcpy(res_data.get(), p_res, p_res_size);
        pushed_data.push_back(std::move(res_data));
        return WBE::MEM_NULL;
    }

    virtual void* get_data(WBE::MemID p_mem_id) override {
        ss << "get_data(" << p_mem_id << ")" << std::endl;
        return nullptr;
    }

    virtual const void* get_data(WBE::MemID p_mem_id) const override {
        ss << "const get_data(" << p_mem_id << ")" << std::endl;
        return nullptr;
    }

protected:
    virtual WBE::RefRes<char> push_res_unit_ptr(const std::string& p_res_key, const void* p_res, size_t p_size) override {
        ss << "push_res_unit_ptr(" << p_res_key << ", pushed_res, " << p_size << ")" << std::endl;
        std::unique_ptr<char[]> res_data = std::make_unique<char[]>(p_size);
        memcpy(res_data.get(), p_res, p_size);
        pushed_res.push_back(std::move(res_data));
        return WBE::MEM_NULL;
    }

};

TEST_F(RULoaderPPM3Test, General) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Load the PPM3 image
    auto result = loader.load<WBE::JSONData>(res_chunk, "test_res", test_ppm3_json_data);
    
    // Verify that the resource chunk methods were called correctly
    std::string output = res_chunk.ss.str();
    EXPECT_TRUE(output.find("push_data(768, pushed_data)") != std::string::npos) 
        << "Expected push_data call for 16x16x3=768 bytes, got: " << output;
    EXPECT_TRUE(output.find("push_res_unit_ptr(test_res, pushed_res,") != std::string::npos)
        << "Expected push_res_unit_ptr call, got: " << output;
    
    // Verify that the RUImage was created correctly
    ASSERT_NE(res_chunk.pushed_res.size(), 0) << "RUImage should have been pushed to chunk";
    const WBE::RUImage* image = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    
    EXPECT_EQ(image->width, 16) << "Image width should be 16";
    EXPECT_EQ(image->height, 16) << "Image height should be 16";
    EXPECT_EQ(image->channel_count, 3) << "Image should have 3 channels (RGB)";
    EXPECT_EQ(image->color_space, WBE::ColorSpace::RGB) << "Color space should be RGB";
    EXPECT_FALSE(image->config.test(WBE::RUImage::IS_HDR)) << "Image should not be HDR";
    
    // Verify that image data was pushed
    EXPECT_NE(res_chunk.pushed_data.size(), 0) << "Image data should have been pushed to chunk";
    
    // Verify some pixel data (first pixel at (0,0) should be red: 255, 0, 0)
    const unsigned char* pixel_data = reinterpret_cast<const unsigned char*>(res_chunk.pushed_data[0].get());
    EXPECT_EQ(pixel_data[0], 255) << "First pixel red component should be 255";
    EXPECT_EQ(pixel_data[1], 0) << "First pixel green component should be 0";
    EXPECT_EQ(pixel_data[2], 0) << "First pixel blue component should be 0";
}

TEST_F(RULoaderPPM3Test, SRGBColorSpace) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Create JSON data with SRGB color space
    std::string srgb_json = R"({
        "path" : "images/test_ppm3.ppm",
        "color_space" : "srgb",
        "flip_h" : false,
        "flip_v" : false
    })";
    
    WBE::ParserJSON parser;
    parser.parse_from_buffer(srgb_json);
    WBE::JSONData srgb_json_data = parser.get_data();
    
    // Load the PPM3 image with SRGB color space
    auto result = loader.load<WBE::JSONData>(res_chunk, "test_srgb", srgb_json_data);
    
    // Verify that the RUImage was created with SRGB color space
    ASSERT_NE(res_chunk.pushed_res.size(), 0) << "RUImage should have been pushed to chunk";
    const WBE::RUImage* image = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    
    EXPECT_EQ(image->color_space, WBE::ColorSpace::SRGB) << "Color space should be SRGB";
    EXPECT_EQ(image->width, 16) << "Image width should be 16";
    EXPECT_EQ(image->height, 16) << "Image height should be 16";
    EXPECT_EQ(image->channel_count, 3) << "Image should have 3 channels (RGB)";
}

TEST_F(RULoaderPPM3Test, HorizontalFlip) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Create JSON data with horizontal flip enabled
    std::string flip_h_json = R"({
        "path" : "images/test_ppm3.ppm",
        "color_space" : "rgb",
        "flip_h" : true,
        "flip_v" : false
    })";
    
    WBE::ParserJSON parser;
    parser.parse_from_buffer(flip_h_json);
    WBE::JSONData flip_h_json_data = parser.get_data();
    
    // Load the PPM3 image with horizontal flip
    auto result = loader.load<WBE::JSONData>(res_chunk, "test_flip_h", flip_h_json_data);
    
    // Verify that the image was loaded correctly
    ASSERT_NE(res_chunk.pushed_res.size(), 0) << "RUImage should have been pushed to chunk";
    ASSERT_NE(res_chunk.pushed_data.size(), 0) << "Image data should have been pushed to chunk";
    
    const WBE::RUImage* image = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    const unsigned char* pixel_data = reinterpret_cast<const unsigned char*>(res_chunk.pushed_data[0].get());
    
    EXPECT_EQ(image->width, 16) << "Image width should be 16";
    EXPECT_EQ(image->height, 16) << "Image height should be 16";
    
    // After horizontal flip, the first pixel should be from the rightmost column of the original first row
    // In our test image, the rightmost pixel of the first row (15,0) should be green (0, 255, 0)
    EXPECT_EQ(pixel_data[0], 0) << "First pixel after h-flip red component should be 0";
    EXPECT_EQ(pixel_data[1], 255) << "First pixel after h-flip green component should be 255";
    EXPECT_EQ(pixel_data[2], 0) << "First pixel after h-flip blue component should be 0";
}

TEST_F(RULoaderPPM3Test, VerticalFlip) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Create JSON data with vertical flip enabled
    std::string flip_v_json = R"({
        "path" : "images/test_ppm3.ppm",
        "color_space" : "rgb",
        "flip_h" : false,
        "flip_v" : true
    })";
    
    WBE::ParserJSON parser;
    parser.parse_from_buffer(flip_v_json);
    WBE::JSONData flip_v_json_data = parser.get_data();
    
    // Load the PPM3 image with vertical flip
    auto result = loader.load<WBE::JSONData>(res_chunk, "test_flip_v", flip_v_json_data);
    
    // Verify that the image was loaded correctly
    ASSERT_NE(res_chunk.pushed_res.size(), 0) << "RUImage should have been pushed to chunk";
    ASSERT_NE(res_chunk.pushed_data.size(), 0) << "Image data should have been pushed to chunk";
    
    const WBE::RUImage* image = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    const unsigned char* pixel_data = reinterpret_cast<const unsigned char*>(res_chunk.pushed_data[0].get());
    
    EXPECT_EQ(image->width, 16) << "Image width should be 16";
    EXPECT_EQ(image->height, 16) << "Image height should be 16";
    
    // After vertical flip, the first pixel should be from the bottom-left of the original image
    // In our test image, the bottom-left (0,15) should be blue (0, 0, 255)
    EXPECT_EQ(pixel_data[0], 0) << "First pixel after v-flip red component should be 0";
    EXPECT_EQ(pixel_data[1], 0) << "First pixel after v-flip green component should be 0";
    EXPECT_EQ(pixel_data[2], 255) << "First pixel after v-flip blue component should be 255";
}

TEST_F(RULoaderPPM3Test, BothFlips) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Create JSON data with both flips enabled
    std::string both_flips_json = R"({
        "path" : "images/test_ppm3.ppm",
        "color_space" : "rgb",
        "flip_h" : true,
        "flip_v" : true
    })";
    
    WBE::ParserJSON parser;
    parser.parse_from_buffer(both_flips_json);
    WBE::JSONData both_flips_json_data = parser.get_data();
    
    // Load the PPM3 image with both flips
    auto result = loader.load<WBE::JSONData>(res_chunk, "test_both_flips", both_flips_json_data);
    
    // Verify that the image was loaded correctly
    ASSERT_NE(res_chunk.pushed_res.size(), 0) << "RUImage should have been pushed to chunk";
    ASSERT_NE(res_chunk.pushed_data.size(), 0) << "Image data should have been pushed to chunk";
    
    const WBE::RUImage* image = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    const unsigned char* pixel_data = reinterpret_cast<const unsigned char*>(res_chunk.pushed_data[0].get());
    
    EXPECT_EQ(image->width, 16) << "Image width should be 16";
    EXPECT_EQ(image->height, 16) << "Image height should be 16";
    
    // After both flips, the first pixel should be from the bottom-right of the original image
    // In our test image, the bottom-right (15,15) should be cyan (0, 255, 255)
    EXPECT_EQ(pixel_data[0], 0) << "First pixel after both flips red component should be 0";
    EXPECT_EQ(pixel_data[1], 255) << "First pixel after both flips green component should be 255";
    EXPECT_EQ(pixel_data[2], 255) << "First pixel after both flips blue component should be 255";
}

TEST_F(RULoaderPPM3Test, OptionalFlipParameters) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Create JSON data without flip parameters (should default to false)
    std::string no_flip_json = R"({
        "path" : "images/test_ppm3.ppm",
        "color_space" : "rgb"
    })";
    
    WBE::ParserJSON parser;
    parser.parse_from_buffer(no_flip_json);
    WBE::JSONData no_flip_json_data = parser.get_data();
    
    // Load the PPM3 image without flip parameters
    auto result = loader.load<WBE::JSONData>(res_chunk, "test_no_flip", no_flip_json_data);
    
    // Verify that the image was loaded correctly (same as no flipping)
    ASSERT_NE(res_chunk.pushed_res.size(), 0) << "RUImage should have been pushed to chunk";
    ASSERT_NE(res_chunk.pushed_data.size(), 0) << "Image data should have been pushed to chunk";
    
    const WBE::RUImage* image = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    const unsigned char* pixel_data = reinterpret_cast<const unsigned char*>(res_chunk.pushed_data[0].get());
    
    EXPECT_EQ(image->width, 16) << "Image width should be 16";
    EXPECT_EQ(image->height, 16) << "Image height should be 16";
    
    // First pixel should be red (same as original - top-left corner)
    EXPECT_EQ(pixel_data[0], 255) << "First pixel red component should be 255";
    EXPECT_EQ(pixel_data[1], 0) << "First pixel green component should be 0";
    EXPECT_EQ(pixel_data[2], 0) << "First pixel blue component should be 0";
}

TEST_F(RULoaderPPM3Test, MultipleLoads) {
    WBE::RULoaderPPM3 loader;
    MockResourceChunk res_chunk;
    
    // Load the same image multiple times with different keys
    auto result1 = loader.load<WBE::JSONData>(res_chunk, "test_res_1", test_ppm3_json_data);
    auto result2 = loader.load<WBE::JSONData>(res_chunk, "test_res_2", test_ppm3_json_data);
    
    // Verify that both images were loaded
    EXPECT_EQ(res_chunk.pushed_res.size(), 2) << "Two RUImages should have been pushed to chunk";
    EXPECT_EQ(res_chunk.pushed_data.size(), 2) << "Two image data blocks should have been pushed to chunk";
    
    // Verify both images have correct properties
    const WBE::RUImage* image1 = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[0].get());
    const WBE::RUImage* image2 = reinterpret_cast<const WBE::RUImage*>(res_chunk.pushed_res[1].get());
    
    EXPECT_EQ(image1->width, 16) << "First image width should be 16";
    EXPECT_EQ(image1->height, 16) << "First image height should be 16";
    EXPECT_EQ(image2->width, 16) << "Second image width should be 16";
    EXPECT_EQ(image2->height, 16) << "Second image height should be 16";
    
    // Verify that the log shows different keys were used
    std::string output = res_chunk.ss.str();
    EXPECT_TRUE(output.find("push_res_unit_ptr(test_res_1, pushed_res,") != std::string::npos)
        << "Expected first load with key test_res_1, got: " << output;
    EXPECT_TRUE(output.find("push_res_unit_ptr(test_res_2, pushed_res,") != std::string::npos)
        << "Expected second load with key test_res_2, got: " << output;
}

#endif
