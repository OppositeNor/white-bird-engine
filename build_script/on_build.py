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
from build_script.reflection.metaparser import WBEMetaparser
from build_script.reflection.reflect import WBEReflector

def reflect(metaparser_clang_args, metadata_path, metadata_cache, sources_indices):
    """Run reflection.

    Args:
        metaparser_clang_args (): The arguments for clang for the metaparser.
        metadata_path (): The path to output metadata.
        metadata_cache (): The path to the metadata cache.
        sources_indices (): The pathes of the source files.
    """
    reflector = WBEReflector(metadata_path)
    metaparser = WBEMetaparser(reflector, metaparser_clang_args, metadata_cache, sources_indices)
    for index_file in sources_indices:
        metaparser.parse(index_file)
    metaparser.export()
    reflector.checks()
    reflector.dump()
