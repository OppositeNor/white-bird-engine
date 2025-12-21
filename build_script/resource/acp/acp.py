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

class WBEACP:
    """The asset conditioning pipeline.
    The ASP will gather all the resource chunks in res/res_chunk, and find all the dependent json
    files, and all the resources the reference to. It will then process each of the resources
    """
    def __init__(self):
        self._compilers = []

    def add_compiler(self, compiler):
        """Add a compiler that could compile resource.

        Args:
            compiler (): 
        """
        self._compilers.append(compiler)

