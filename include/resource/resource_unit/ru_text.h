#ifndef __WBE_RU_TEXT_H__
#define __WBE_RU_TEXT_H__

#include "core/allocator/allocator.h"
#include "resource_unit.h"
#include <cstddef>
namespace WhiteBirdEngine {

/**
 * @class RUText
 * @brief Text resource unit. contains encoding type, size (in bytes)
 * and a MemID pointing to the data chunk that stores the data of the string.
 *
 * How image should be stored is that the data should always be pushed before
 * pushing this resource unit, and the release process should always be backwards
 * (first release the resource unit, and then release the data) It is recommended
 * to store the image data right before the text resource unit in a stack allocator.
 */
struct RUText : public ResourceUnit {
    // Types of encoding
    enum class EncodeType {
        // UTF8 (most recommended)
        UTF8,
        // UTF16 little endian
        UTF16LE,
        // UTF16 big endian
        UTF16BE,
        // UTF32 little endian
        UTF32LE,
        // UTF32 big endian
        UTF32BE
    };

    // The encoding type.
    EncodeType encode_type = EncodeType::UTF8;
    // The size of the string (in bytes)
    size_t size;
    // The memid of the text data.
    MemID data;
};

}

#endif
