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

def reflect(p_metaparser_clang_args, p_metadata_path, p_metadata_cache, p_sources_indices):
    """Run reflection.

    Args:
        p_metaparser_clang_args (): The arguments for clang for the metaparser.
        p_metadata_path (): The path to output metadata.
        p_metadata_cache (): The path to the metadata cache.
        p_sources_indices (): The pathes of the source files.
    """
    reflector = WBEReflector(p_metadata_path)
    metaparser = WBEMetaparser(reflector, p_metaparser_clang_args, p_metadata_cache, p_sources_indices)
    for index_file in p_sources_indices:
        metaparser.parse(index_file)
    metaparser.export()
    reflector.checks()
    reflector.dump()
