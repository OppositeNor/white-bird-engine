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
#ifndef WBE_FILE_RO_MATERIAL_VK_HH
#define WBE_FILE_RO_MATERIAL_VK_HH

#include "function/render_vk/render_object_vk.hh"
namespace WhiteBirdEngine {

class WBE_META(WBE_RENDER_OBJECT) ROMaterialVK : public RenderObjectVK {
public:

    void bind();
};
}

#endif
