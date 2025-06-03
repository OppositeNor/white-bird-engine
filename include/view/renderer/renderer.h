#ifndef __WBE_RENDERER_H__
#define __WBE_RENDERER_H__

namespace WhiteBirdEngine {

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() {}

    virtual void render(double p_delta) = 0;
};

}

#endif
