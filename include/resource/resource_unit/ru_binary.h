#ifndef __WBE_RU_BINARY_H__
#define __WBE_RU_BINARY_H__

#include "core/allocator/allocator.h"
#include "resource_unit.h"
#include <stddef.h>
namespace WhiteBirdEngine {

/**
 * @class RUBinary
 * @brief Binary resource unit. contains a size (in bytes) and a MemID pointing to
 * the data chunk that stores the data of the binary data.
 *
 * The data should always be pushed before pushing this resource unit, and the
 * release process should always be backwards (first release the resource unit, and
 * then release the data) It is recommended to store the image data right before the
 * text resource unit in a stack allocator.
 *
 */
struct RUBinary : public ResourceUnit {
    // The size of the binary data size (in bytes).
    size_t size;
    // The binary data.
    MemID data;
};

}

#endif
