#ifndef __WBE_RENDERER_DUMMY_H__
#define __WBE_RENDERER_DUMMY_H__

#include "function/render/renderer.hh"
namespace WhiteBirdEngine {

/**
 * @class RendererDummy
 * @brief A dummy renderer that does nothing.
 *
 */
class RendererDummy : public Renderer {
public:
    RendererDummy() = default;
    virtual ~RendererDummy() override {}
    // TODO
};

}



#endif
