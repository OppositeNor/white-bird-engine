#include "resource/resource_chunk/resource_chunk.h"
#include "resource/resource_unit/ru_text.h"
#include <stdexcept>

namespace WhiteBirdEngine {

Path ResourceChunk::get_resource_path(const std::string& p_res_path) {
    return Path(); // TODO
}

RUText::EncodeType ResourceChunk::get_encode_type(const std::string& p_str) {
    if (p_str == "utf8") {
        return RUText::EncodeType::UTF8;
    }
    if (p_str == "utf16le") {
        return RUText::EncodeType::UTF16LE;
    }
    if (p_str == "utf16be") {
        return RUText::EncodeType::UTF16BE;
    }
    if (p_str == "utf32le") {
        return RUText::EncodeType::UTF32LE;
    }
    if (p_str == "utf32be") {
        return RUText::EncodeType::UTF32BE;
    }
    throw std::runtime_error("Encoding type: " + p_str + " is not recognized.");
}

}

