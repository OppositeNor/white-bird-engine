#ifndef __WBE_RENDERER_HH__
#define __WBE_RENDERER_HH__

#include "utils/interface/singleton.hh"
namespace WhiteBirdEngine {

class Renderer : public Singleton<Renderer> {
public:
    Renderer() = default;
    virtual ~Renderer() {}
};

}

#endif
