#ifndef __RENDERER_H__
#define __RENDERER_H__

namespace PencilEngine {

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() {}

    virtual void render(double p_delta) = 0;
};

}

#endif
