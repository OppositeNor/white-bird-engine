#ifndef __WBE_RO_BUFFER_HH__
#define __WBE_RO_BUFFER_HH__

#include "function/render_vk/render_object_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class WBE_META(WBE_RENDER_OBJECT) ROBufferVK final : public RenderObjectVK {
public:
    struct Builder {
        // TODO
    };
    // TODO

    VkBuffer get_buffer() const {
        return buffer;
    }

private:
    VkBuffer buffer;
};
}

#endif
