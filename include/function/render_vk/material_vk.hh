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
#ifndef WBE_FILE_MATERIAL_VK_HH
#define WBE_FILE_MATERIAL_VK_HH

#include "core/allocator/allocator.hh"
#include "function/render/material.hh"
namespace WhiteBirdEngine {

class MaterialVK final : public Material {
public:
    // TODO
};

class MaterialFactoryVK final : public MaterialFactory {
public:
    WBE_R6_DEFAULT_OVERRIDE(MaterialFactoryVK)
    // TODO
    virtual Ref<Material> create(RefRes<RUMaterial> p_material) override {
        (void)p_material;
        return MEM_NULL; // TODO
    }
};


} // namespace WhiteBirdEngine

#endif
