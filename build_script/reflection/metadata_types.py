# Copyright 2025 OppositeNor
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from pydantic import BaseModel, Field

class WBEFieldMetadata(BaseModel):
    """Field metadata.

    Attributes: 
        field_name: The name of the field.
        field_type: The type of the field.
    """
    attribute : str = ""
    field_name : str = ""
    field_type : str = ""

class WBEMethodMetadata(BaseModel):
    """Method metadata.

    Attributes: 
        method_name: The name of the method.
        args_name: The name of the arguments.
        args_type: The types of the arguments.
        ret_type: The return type.
    """
    attribute : str = ""
    method_name : str = ""
    args_name : list[str] = Field(default_factory=list)
    args_type : list[str] = Field(default_factory=list)
    ret_type : str = ""

class WBEClassMetadata(BaseModel):
    """Class metadata.

    Attributes: 
        class_name: The name of the class.
        fields: The fields of the class.
        methods: The methods of the class.
    """
    attribute : str = ""
    class_name : str = ""
    fields : list[WBEFieldMetadata] = Field(default_factory=list)
    methods : list[WBEMethodMetadata] = Field(default_factory=list)

class WBELabelMetadata(BaseModel):
    """Label metadata.

    Attributes: 
        attribute: The attribute of the label.
        label_name: The name of the label.
    """
    attribute : str = ""
    label_name : str = ""

class WBEComponentMetadata(BaseModel):
    """Component metadata.

    Attributes: 
        struct_name: The name of the struct.
        fields: The name of the fields.
        types: The types of the fields
    """
    struct_name : str = ""
    fields : list[WBEFieldMetadata] = Field(default_factory=list)

class WBEFileMetadata(BaseModel):
    """Metadata in a single C++ file.

    Attributes: 
        file_path: The path of the file.
        hashcode: The hashcode of the file's content.
        deps: The dependencies of the file. Usually from include or import module.
        labels: The registered labels metadata.
        classes: The registered classes metadata.
    """
    file_path : str = ""
    hashcode : str = ""
    deps : list[str] = Field(default_factory=list)
    labels : list[WBELabelMetadata] = Field(default_factory=list)
    classes : list[WBEClassMetadata] = Field(default_factory=list)
    components : list[WBEComponentMetadata] = Field(default_factory=list)

class WBEMetadata(BaseModel):
    """Metadata.

    Attributes:
        components_headers: The header files that the components are defined in.
        channels: The registrated channels.
        classes: The registrated classes.
        components: The registrated components.
    """
    components_headers : list[str] = Field(default_factory=list[str])
    channels : dict = Field(default_factory=dict)
    classes : list[WBEClassMetadata] = Field(default_factory=list)
    components : list[WBEComponentMetadata] = Field(default_factory=list)
