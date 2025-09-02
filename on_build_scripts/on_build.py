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
import importlib.util
import os
import sys
from reflection.metaparser import WBEMetaparser
from reflection.reflect import WBEReflector

clang_args = [
    "-std=c++20",
    "-I../include",
    "-DWBE_REFLECTION_PARSER"
]

def main():
    if (len(sys.argv) != 4):
        print("Usage: " + sys.argv[0] + " <res_path> <source_index_file> <metadata_cache>")
        exit(-1)
    print("White Bird Engine build script start.")
    _reflect(clang_args)
    print("White Bird Engine build script finish.")

def _reflect(p_metaparser_clang_args):
    """Run reflection."""
    reflector = WBEReflector(sys.argv[1])
    indices = _load_indices(sys.argv[2])
    metaparser = WBEMetaparser(reflector, p_metaparser_clang_args, sys.argv[3])
    for index_file in indices:
        metaparser.parse(index_file)
    metaparser.export()
    reflector.checks()
    reflector.dump()

def _load_indices(p_index_file : str):
    if not os.path.exists(p_index_file):
        raise FileNotFoundError(f"Python file not found: {p_index_file}")
    spec = importlib.util.spec_from_file_location("indices", p_index_file)
    if spec is None:
        raise RuntimeError(f"Failed to create module spec for file: {p_index_file}")
    module_config = importlib.util.module_from_spec(spec)
    if spec.loader is None:
        raise RuntimeError(f"Failed to create module spec for file: {p_index_file}")
    spec.loader.exec_module(module_config)
    return module_config.indices

if __name__ == "__main__":
    main()
