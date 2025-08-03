#ifndef __WBE_RU_TEXT_HH__
#define __WBE_RU_TEXT_HH__

#include "core/allocator/allocator.hh"
#include "resource_unit.hh"
#include "utils/defs.hh"
#include <cstddef>
namespace WhiteBirdEngine {

/**
 * @class RUText
 * @brief Text resource unit. contains encoding type, size (in bytes, excluding
 * the NUL terminater) and a MemID pointing to the data chunk that stores the data
 * of the string (ends with a NUL character).
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 */
struct RUText : public ResourceUnit {

    // The encoding type.
    EncodeType encode_type = EncodeType::UTF8;
    // The size of the string (in bytes)
    size_t size;
    // The memid of the text data.
    MemID data;
};

}

#endif
