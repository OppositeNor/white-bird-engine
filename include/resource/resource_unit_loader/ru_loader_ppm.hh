// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_RU_LOADER_PPM_HH__
#define __WBE_RU_LOADER_PPM_HH__

#include "platform/file_system/path.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_unit_loader/ru_loader_ppm3.hh"
#include <stdexcept>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RULoaderPPM
 * @brief Resource unit loader for ppm images.
 *
 */
class RULoaderPPM : public ResourceUnitLoader<RULoaderPPM> {
public:
    RULoaderPPM() = default;
    ~RULoaderPPM() {}
    RULoaderPPM(const RULoaderPPM&) = delete;
    RULoaderPPM(RULoaderPPM&&) = delete;
    RULoaderPPM& operator=(const RULoaderPPM&)= delete;
    RULoaderPPM& operator=(RULoaderPPM&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        // TODO: Test
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));
        auto header = get_ppm_header(path);
        if (header == "P3") {
            return loader_ppm3.load(p_chunk, p_parser_data, p_allocator);
        }
        throw std::runtime_error("Failed to load PPM: header: " + header + " not supported by White Bird Enigne.");
    }

private:
    RULoaderPPM3 loader_ppm3;
    std::string get_ppm_header(const Path& p_path);
};


}

#endif
