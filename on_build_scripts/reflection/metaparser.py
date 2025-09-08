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
from utils import hash_file, hash_str_sha256
from reflection.reflect import WBEReflector
import clang.cindex
import json
from reflection.metadata_types import WBEClassMetadata, WBEFileMetadata, WBELabelMetadata

class WBEMetaparser:
    """Metaparser

    Attributes: 
        reflector: The reflector.
        clang_args: The arguments for clang.
        cache_dir: The cache directory.
    """
    def __init__(self, p_reflector : WBEReflector, p_clang_args, p_cache_dir, p_sources):
        """Constructor.

        Args:
            p_reflector: The reflector for the metaparser.
            p_clang_args: Clang arguments for parsing.
            p_cache_dir: The cache directory.
        """
        self.reflector = p_reflector
        self.clang_args = p_clang_args
        self.cache_dir = p_cache_dir
        self.sources = p_sources
        self._metadata = {}

    def parse(self, p_cpp_file_path : str):
        """Parse the C++ file.

        Args:
            p_cpp_file_path: The path to the C++ file.
        """
        os.makedirs(self.cache_dir, exist_ok=True)
        file_cache_path = os.path.join(self.cache_dir, f"{hash_str_sha256(p_cpp_file_path)}.json")
        self._get_metadata(p_cpp_file_path, file_cache_path)

    def export(self):
        """Export"""
        self._export_metadata()

    def _get_metadata(self, p_cpp_file_path, p_cache_path):
        preloaded = self._metadata.get(p_cpp_file_path)
        if preloaded is not None:
            return preloaded
        if os.path.exists(p_cache_path):
            return self._register_from_cache(p_cpp_file_path, p_cache_path)
        else:
            return self._register_from_clang(p_cpp_file_path, p_cache_path)

    def _register_from_cache(self, p_cpp_file_path, p_cache_path):
        try:
            with open(p_cache_path) as f:
                data = json.load(f)
            metadata = WBEFileMetadata(**data)
            file_content_hashcode = hash_file(p_cpp_file_path)
            if metadata.hashcode != file_content_hashcode:
                return self._register_from_clang(p_cpp_file_path, p_cache_path)
            self._metadata[p_cpp_file_path] = metadata
            return metadata
        except:
            return self._register_from_clang(p_cpp_file_path, p_cache_path)

    def _export_metadata(self):
        for _, metadata in self._metadata.items():
            for label in metadata.labels:
                if "WBE_CHANNEL" in label.attribute.split(", "):
                    self.reflector.register_channel(label.label_name)

    def _register_from_clang(self, p_cpp_file_path, p_cache_path):
        print(f"WBEMetaparser: parsing {p_cpp_file_path}")
        index = clang.cindex.Index.create()
        tu = index.parse(
            p_cpp_file_path,
            args=self.clang_args,
            options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        )
        metadata = WBEFileMetadata()
        metadata.deps = self._get_include_deps(tu)
        metadata.file_path = p_cpp_file_path
        self._register_labels(tu, metadata)
        metadata.hashcode = hash_file(p_cpp_file_path)
        self._metadata[p_cpp_file_path] = metadata
        with open(p_cache_path, "w") as f:
            json.dump(metadata.model_dump(), f, indent=4)
        return metadata

    def _register_labels(self, p_tu, p_metadata : WBEFileMetadata):
        for attribute, data in self._visit_attributes(p_metadata, p_tu.cursor):
            getattr(p_metadata, attribute).append(data)

    def _visit_attributes(self, p_metadata, p_cursor):
        if p_cursor.kind == clang.cindex.CursorKind.VAR_DECL:
            for attr in p_cursor.get_children():
                if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
                    if not self._any_all(p_metadata, lambda p_metadata:
                            any(p_cursor.spelling == label.label_name for label in p_metadata.labels)):
                        yield "labels", WBELabelMetadata(label_name=p_cursor.spelling, attribute=attr.spelling)
        elif p_cursor.kind == clang.cindex.CursorKind.CLASS_DECL:
            for attr in p_cursor.get_children():
                if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
                    if not self._any_all(p_metadata, lambda p_metadata:
                            any(p_cursor.spelling == metad_class.class_name for metad_class in p_metadata.classes)):
                        yield "classes", self._get_class_metadata(p_cursor, attr.displayname)

        for child in p_cursor.get_children():
            if child.kind != clang.cindex.CursorKind.ANNOTATE_ATTR:
                yield from self._visit_attributes(p_metadata, child)

    def _get_class_metadata(self, p_cursor, p_attribute):
        result = WBEClassMetadata()
        result.class_name = p_cursor.spelling
        result.attribute = p_attribute
        # TODO
        return WBEClassMetadata(class_name=p_cursor.spelling)

    def _read_from_cache(self, p_cpp_file_path, p_cache_path):
        with open(p_cache_path) as f:
            data = json.load(f)
        metadata = WBEFileMetadata(**data)
        self._metadata[p_cpp_file_path] = metadata
        return metadata

    def _get_include_deps(self, p_tu):
        deps = set()
        for inclusion in p_tu.get_includes():
            incl_path_abs = os.path.abspath(inclusion.include.name)
            if incl_path_abs in self.sources:
                deps.add(incl_path_abs)
        return list(deps)

    def _any_all(self, p_metadata, p_predicate):
        if p_predicate(p_metadata):
            return True
        for dep in p_metadata.deps:
            dep_metadata_file_path = os.path.join(self.cache_dir, f"{hash_str_sha256(dep)}.json")
            dep_metadata = self._get_metadata(dep, dep_metadata_file_path)
            if p_predicate(dep_metadata):
                return True
        return False

