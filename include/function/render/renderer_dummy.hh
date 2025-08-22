/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
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

};

}



#endif
