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
#ifndef __WBE_MATERIAL_HH__
#define __WBE_MATERIAL_HH__

#include "core/memory/reference_strong.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdint>
namespace WhiteBirdEngine {

class Material {
    // TODO
};

class MaterialFactory {
public:
    // TODO
    virtual Ref<Material> create(RefRes<RUMaterial> p_material) = 0;
};

} // namespace WhiteBirdEngine

#endif
