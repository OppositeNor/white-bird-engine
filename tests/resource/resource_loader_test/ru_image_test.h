#ifndef __WBE_RU_IMAGE_TEST_H__
#define __WBE_RU_IMAGE_TEST_H__

#include <gtest/gtest.h>

#include "global/global.h"
#include "resource/resource_chunk/resource_chunk.h"
#include "resource/resource_manager/resource_manager.h"
#include "resource/resource_unit_loader/ru_loader_png.h"

namespace WBE = WhiteBirdEngine;

// TODO


TEST(RUImageLoaderTest, PNGLoaderTest) {
    WBE::Global global(0, nullptr, WBE::Directory({"test_env"}));
    WBE::ResourceChunk<WBE::ResourceChunkLoadFile> res_chunk;
    WBE::RULoaderPNG png_loader;
}

#endif
