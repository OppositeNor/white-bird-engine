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
from typing import List
from utils import hash_file, hash_str_sha256
from reflection.reflect import WBEReflector
import clang.cindex
import json
from dataclasses import dataclass

@dataclass
class FileMetadata:
    labels : List[str]


class WBEMetaparser:

    """Metaparser

    Attributes: 
        reflector: The reflector.
        clang_args: The arguments for clang.
        cache_dir: The cache directory.
    """
    def __init__(self, p_reflector : WBEReflector, p_clang_args, p_cache_dir):
        """Constructor.

        Args:
            p_reflector: The reflector for the metaparser.
            p_clang_args: Clang arguments for parsing.
            p_cache_dir: The cache directory.
        """
        self.reflector = p_reflector
        self.clang_args = p_clang_args
        self.cache_dir = p_cache_dir
        self._files_content_hashcodes = self._get_files_content_hashcodes()
        self._metadata = {}

    def parse(self, p_cpp_file_path : str):
        """Parse the C++ file.

        Args:
            p_cpp_file_path: The path to the C++ file.
        """
        file_hashcode = hash_file(p_cpp_file_path)
        if self._files_content_hashcodes.get(p_cpp_file_path) == file_hashcode:
            self._read_from_cache(p_cpp_file_path)
        else:
            self._register_from_clang(p_cpp_file_path)
        self._files_content_hashcodes[p_cpp_file_path] = file_hashcode

    def export(self):
        """Export"""
        self._export_cache()
        self._export_metadata()

    def _export_metadata(self):
        for _, metadata in self._metadata.items():
            self._register_metadata(metadata)

    def _export_cache(self):
        os.makedirs(self.cache_dir, exist_ok=True)

        for file_path, metadata in self._metadata.items():
            file_name_hash = hash_str_sha256(file_path)
            cache_file_path = os.path.join(self.cache_dir, f"{file_name_hash}.json")
            data = metadata.__dict__

            with open(cache_file_path, "w") as f:
                json.dump(data, f, indent=4)

        content_hash_file = os.path.join(self.cache_dir, "content_hashcodes.json")
        with open(content_hash_file, "w") as f:
            json.dump(self._files_content_hashcodes, f, indent=4)

    def _get_files_content_hashcodes(self):
        file_path = os.path.join(self.cache_dir, "content_hashcodes.json")
        if not os.path.exists(file_path):
            return {}
        with open(file_path) as f:
            return json.load(f)

    def _register_from_clang(self, p_cpp_file_path):
        print(f"WBEMetaparser: parsing {p_cpp_file_path}...")
        metadata = FileMetadata([])
        index = clang.cindex.Index.create()
        tu = index.parse(
            p_cpp_file_path,
            args=self.clang_args,
            options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        )
        self._register_labels(tu, metadata)
        self._metadata[p_cpp_file_path] = metadata

    def _register_labels(self, p_tu, p_metadata : FileMetadata):
        for label in self._visit_labels(p_tu.cursor):
            p_metadata.labels.append(label)

    def _visit_labels(self, p_cursor):
        if p_cursor.kind == clang.cindex.CursorKind.VAR_DECL:
            for attr in p_cursor.get_children():
                if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR and attr.displayname == "WBE_LABEL":
                    yield p_cursor.spelling
        for child in p_cursor.get_children():
            if child.kind != clang.cindex.CursorKind.ANNOTATE_ATTR:
                yield from self._visit_labels(child)

    def _read_from_cache(self, p_cpp_file_path):
        file_name_hash = hash_str_sha256(p_cpp_file_path)
        file_metadata_path = os.path.join(self.cache_dir, f"{file_name_hash}.json")
        if not os.path.exists(file_metadata_path):
            self._register_from_clang(p_cpp_file_path)
        else:
            with open(file_metadata_path) as f:
                data = json.load(f)
            self._metadata[p_cpp_file_path] = FileMetadata(**data)

    def _register_metadata(self, p_metadata : FileMetadata):
        for label in p_metadata.labels:
            self.reflector.register_channel(label)

