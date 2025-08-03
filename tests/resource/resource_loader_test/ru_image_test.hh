#ifndef __WBE_RU_IMAGE_TEST_HH__
#define __WBE_RU_IMAGE_TEST_HH__

#include <gtest/gtest.h>

#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_unit_loader/ru_loader_png.hh"

namespace WBE = WhiteBirdEngine;

// TODO


TEST(RUImageLoaderTest, PNGLoaderTest) {
    WBE::Global global(0, nullptr, WBE::Directory({"test_env"}));
    WBE::ResourceChunk<WBE::ResourceChunkLoadFile> res_chunk;
    WBE::RULoaderPNG png_loader;
}

#endif
