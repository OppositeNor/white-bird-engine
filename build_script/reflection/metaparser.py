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
import os
from build_script.utils import hash_file, hash_str_sha256
from build_script.reflection.reflect import WBEReflector
import clang.cindex
import json
from build_script.reflection.metadata_types import WBEClassMetadata, WBEComponentMetadata, WBEFieldMetadata, WBEFileMetadata, WBELabelMetadata

WBE_REFLECT = "WBE_REFLECT"
WBE_COMPONENT = "WBE_COMPONENT"

class WBEMetaparser:
    """Metaparser

    Attributes: 
        reflector: The reflector.
        clang_args: The arguments for clang.
        cache_dir: The cache directory.
    """
    def __init__(self, reflector : WBEReflector, clang_args, cache_dir, sources):
        """Constructor.

        Args:
            reflector: The reflector for the metaparser.
            clang_args: Clang arguments for parsing.
            cache_dir: The cache directory.
        """
        self.reflector = reflector
        self.clang_args = clang_args
        self.cache_dir = cache_dir
        self.sources = sources
        self._metadata = {}
        self._components_headers = set()

    def parse(self, cpp_file_path : str):
        """Parse the C++ file.

        Args:
            cpp_file_path: The path to the C++ file.
        """
        file_cache_path = os.path.join(self.cache_dir, f"{hash_str_sha256(cpp_file_path)}.json")
        self._get_metadata(cpp_file_path, file_cache_path)

    def export(self):
        """Export"""
        self._export_metadata()

    def _get_metadata(self, cpp_file_path, cache_path):
        preloaded = self._metadata.get(cpp_file_path)
        if preloaded is not None:
            result = preloaded
        if os.path.exists(cache_path):
            result = self._register_from_cache(cpp_file_path, cache_path)
        else:
            result = self._register_from_clang(cpp_file_path, cache_path)
        if len(result.components) > 0:
            self._components_headers.add(cpp_file_path)
        return result

    def _register_from_cache(self, cpp_file_path, cache_path):
        try:
            with open(cache_path) as f:
                data = json.load(f)
            metadata = WBEFileMetadata(**data)
            file_content_hashcode = hash_file(cpp_file_path)
            if metadata.hashcode != file_content_hashcode:
                return self._register_from_clang(cpp_file_path, cache_path)
            self._metadata[cpp_file_path] = metadata
            return metadata
        except:
            return self._register_from_clang(cpp_file_path, cache_path)

    def _export_metadata(self):
        for _, metadata in self._metadata.items():
            for label in metadata.labels:
                split_attribute = label.attribute.split(", ")
                if "WBE_CHANNEL" in split_attribute:
                    self.reflector.register_channel(label.label_name)
            for cxx_class in metadata.classes:
                self.reflector.register_class(cxx_class)
            for component in metadata.components:
                self.reflector.register_component(component)
        self.reflector.register_components_headers(self._components_headers)


    def _register_from_clang(self, cpp_file_path, cache_path):
        print(f"WBEMetaparser: parsing {cpp_file_path}")
        index = clang.cindex.Index.create()
        tu = index.parse(
            cpp_file_path,
            args=self.clang_args,
            options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        )
        metadata = WBEFileMetadata()
        metadata.deps = self._get_include_deps(tu)
        metadata.file_path = cpp_file_path
        self._register_labels(tu, metadata)
        metadata.hashcode = hash_file(cpp_file_path)
        self._metadata[cpp_file_path] = metadata
        with open(cache_path, "w") as f:
            json.dump(metadata.model_dump(), f, indent=4)
        return metadata

    def _register_labels(self, tu, metadata : WBEFileMetadata):
        for attribute, data in self._visit_attributes(metadata, tu.cursor):
            getattr(metadata, attribute).append(data)

    def _visit_attributes(self, metadata, cursor):
        if cursor.kind == clang.cindex.CursorKind.VAR_DECL:
            yield from self._handle_visit_var_decl(metadata, cursor)
        elif cursor.kind == clang.cindex.CursorKind.CLASS_DECL:
            yield from self._handle_visit_class_decl(metadata, cursor)
        elif cursor.kind == clang.cindex.CursorKind.STRUCT_DECL:
            yield from self._handle_visit_struct_decl(metadata, cursor)

        for child in cursor.get_children():
            if child.kind != clang.cindex.CursorKind.ANNOTATE_ATTR:
                yield from self._visit_attributes(metadata, child)

    def _handle_visit_var_decl(self, metadata, cursor):
        for attr in cursor.get_children():
            if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
                if not self._any_all(metadata, lambda metadata:
                        any(cursor.spelling == label.label_name for label in metadata.labels)):
                    yield "labels", WBELabelMetadata(label_name=cursor.spelling, attribute=attr.spelling)

    def _handle_visit_class_decl(self, metadata, cursor):
        for attr in cursor.get_children():
            if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
                if not self._any_all(metadata, lambda metadata:
                        any(cursor.spelling == metad_class.class_name for metad_class in metadata.classes)):
                    yield "classes", self._get_class_metadata(cursor, attr.spelling)

    def _handle_visit_struct_decl(self, metadata, cursor):
        for attr in cursor.get_children():
            if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
                if attr.spelling == WBE_COMPONENT:
                    if not self._any_all(metadata, lambda metadata:
                            any(cursor.spelling == metad_struct.struct_name for metad_struct in metadata.components)):
                        yield "components", self._get_component_metadata(cursor)

    def _read_from_cache(self, cpp_file_path, cache_path):
        with open(cache_path) as f:
            data = json.load(f)
        metadata = WBEFileMetadata(**data)
        self._metadata[cpp_file_path] = metadata
        return metadata

    def _get_include_deps(self, tu):
        deps = set()
        for inclusion in tu.get_includes():
            incl_path_abs = os.path.abspath(inclusion.include.name)
            if incl_path_abs in self.sources:
                deps.add(incl_path_abs)
        return list(deps)

    def _any_all(self, metadata, predicate):
        if predicate(metadata):
            return True
        for dep in metadata.deps:
            dep_metadata_file_path = os.path.join(self.cache_dir, f"{hash_str_sha256(dep)}.json")
            dep_metadata = self._get_metadata(dep, dep_metadata_file_path)
            if predicate(dep_metadata):
                return True
        return False

    def _get_class_metadata(self, cursor, attribute):
        result = WBEClassMetadata()
        result.class_name = cursor.spelling
        result.attribute = attribute
        # TODO
        return WBEClassMetadata(class_name=cursor.spelling)

    def _get_component_metadata(self, cursor):
        result = WBEComponentMetadata()
        result.struct_name = cursor.spelling
        for field in cursor.get_children():
            if field.kind != clang.cindex.CursorKind.FIELD_DECL:
                continue
            for attr in field.get_children():
                if attr.kind != clang.cindex.CursorKind.ANNOTATE_ATTR or attr.spelling != WBE_REFLECT:
                    continue
                result.fields.append(WBEFieldMetadata(attribute=attr.spelling, field_name=field.spelling, field_type=field.type.spelling))
        return result



