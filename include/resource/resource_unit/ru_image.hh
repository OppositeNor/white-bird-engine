#ifndef __WBE_RU_IMAGE_HH__
#define __WBE_RU_IMAGE_HH__

#include "core/allocator/allocator.hh"
#include "resource_unit.hh"
#include "utils/utils.hh"
#include <bitset>
#include <cstdint>
namespace WhiteBirdEngine {

/**
 * @class RUImage
 * @brief Image resource unit. Contails descriptions of an image
 * resource and a MemID to the resource of the image data.
 *
 * The image data should always be pushed as data before pushing this
 * description, and the release process should always be backwards
 * (first release the resource unit and then remove the data). It is
 * recommended to store the image data right before the image description
 * in a stack allocator.
 *
 */
struct RUImage : public ResourceUnit {
    enum ImageConfigBit {
        // Is the image an HDR image (32 bit instead of 8 bit)
        IS_HDR,
        // To track the total number of bits.
        TOTAL_CONFIG_BIT
    };

    // The width of the image (in pixels).
    uint32_t width;
    // The height of the image (in pixels).
    uint32_t height;
    // The channel count of the image.
    uint32_t channel_count;
    // The color space of the image.
    ColorSpace color_space;
    // Image configurations.
    std::bitset<TOTAL_CONFIG_BIT> config;
    // The memory ID of the image data.
    MemID image_data;
};

}

#endif
