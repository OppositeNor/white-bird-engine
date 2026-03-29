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
#ifndef WBE_FILE_RESOURCE_DEFS_HH
#define WBE_FILE_RESOURCE_DEFS_HH
#include "core/reflection/serializable.hh"
#include "core/reflection/reflection_defs.hh"

namespace WhiteBirdEngine {
// NOLINTBEGIN
struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataBinding : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataBinding)
    WBE_META(WBE_REFLECT)
    std::string kind;
    WBE_META(WBE_REFLECT)
    int32_t index = -1;
    WBE_META(WBE_REFLECT)
    int32_t count = -1;
    WBE_META(WBE_REFLECT)
    int32_t space = -1;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataTypeAtom : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataTypeAtom)
    WBE_META(WBE_REFLECT)
    std::string kind;
    WBE_META(WBE_REFLECT)
    std::string scalarType;
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    int32_t elementCount = -1;
    WBE_META(WBE_REFLECT)
    std::string elementScalarType;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataTypeField : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataTypeField)
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    ShaderMetadataTypeAtom type;
    WBE_META(WBE_REFLECT)
    std::string stage;
    WBE_META(WBE_REFLECT)
    ShaderMetadataBinding binding;
    WBE_META(WBE_REFLECT)
    std::string semanticName;
    WBE_META(WBE_REFLECT)
    int32_t semanticIndex = -1;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataType : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataType)
    WBE_META(WBE_REFLECT)
    std::string kind;
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    std::vector<ShaderMetadataTypeField> fields;
    WBE_META(WBE_REFLECT)
    int32_t elementCount = -1;
    WBE_META(WBE_REFLECT)
    ShaderMetadataTypeAtom elementType;
    WBE_META(WBE_REFLECT)
    std::string scalarType;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataParameter : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataParameter)
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    std::string stage;
    WBE_META(WBE_REFLECT)
    ShaderMetadataBinding binding;
    WBE_META(WBE_REFLECT)
    ShaderMetadataType type;
    WBE_META(WBE_REFLECT)
    std::string semanticName;
    WBE_META(WBE_REFLECT)
    int32_t semanticIndex = -1;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataResult : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataResult)
    WBE_META(WBE_REFLECT)
    std::string stage;
    WBE_META(WBE_REFLECT)
    ShaderMetadataBinding binding;
    WBE_META(WBE_REFLECT)
    std::string semanticName;
    WBE_META(WBE_REFLECT)
    int32_t semanticIndex = -1;
    WBE_META(WBE_REFLECT)
    ShaderMetadataType type;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadataEntryPoint : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadataEntryPoint)
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    std::string stage;
    WBE_META(WBE_REFLECT)
    std::vector<ShaderMetadataParameter> parameters;
    WBE_META(WBE_REFLECT)
    ShaderMetadataResult result;
    WBE_META(WBE_REFLECT)
    std::vector<ShaderMetadataBinding> bindings;
};

struct WBE_META(WBE_SERIALIZABLE) ShaderMetadata : public Serializable {
    WBE_DECL_SERIALIZABLE(ShaderMetadata)
    WBE_META(WBE_REFLECT)
    std::vector<ShaderMetadataParameter> parameters;
    WBE_META(WBE_REFLECT)
    std::vector<ShaderMetadataEntryPoint> entryPoints;
    WBE_META(WBE_REFLECT)
    int32_t bindlessSpaceIndex = -1;
};
// NOLINTEND

} // namespace WhiteBirdEngine
#endif
