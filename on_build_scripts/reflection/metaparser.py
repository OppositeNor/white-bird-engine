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

from reflection.reflect import WBEReflector
import clang.cindex

class WBEMetaparser:
    """Metaparser

    Attributes: 
        reflector: The reflector.
        p_clang_args: The arguments for clang.
    """
    def __init__(self, p_reflector : WBEReflector, p_clang_args):
        """Constructor.

        Args:
            p_reflector: The reflector for the metaparser.
        """
        self.reflector = p_reflector
        self.clang_args = p_clang_args

    def parse(self, p_cpp_file_path : str):
        """Parse the C++ file.

        Args:
            p_cpp_file_path: The path to the C++ file.
        """
        print(f"WBEMetaparser: parsing {p_cpp_file_path}")
        index = clang.cindex.Index.create()
        tu = index.parse(
            p_cpp_file_path,
            args=self.clang_args,
            options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        )
        self._register_labels(tu)

    def _register_labels(self, tu):
        for label in self._visit_labels(tu.cursor):
            self.reflector.register_channel(label)

    def _visit_labels(self, cursor):
        if cursor.kind == clang.cindex.CursorKind.VAR_DECL:
            for attr in cursor.get_children():
                if attr.kind == clang.cindex.CursorKind.ANNOTATE_ATTR and attr.displayname == "WBE_LABEL":
                    yield cursor.spelling
        for child in cursor.get_children():
            if child.kind != clang.cindex.CursorKind.ANNOTATE_ATTR:
                yield from self._visit_labels(child)

