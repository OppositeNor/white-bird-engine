// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_RENDERER_HH__
#define __WBE_RENDERER_HH__

#include "utils/interface/singleton.hh"
#include <cstdint>
namespace WhiteBirdEngine {

using RenderID = uint32_t;

class Renderer : public Singleton<Renderer> {
public:
    Renderer() = default;
    virtual ~Renderer() {}

};

}

#endif
