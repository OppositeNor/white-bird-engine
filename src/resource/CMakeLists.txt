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
file(GLOB wbe_resource_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_subdirectory(resource_manager)
add_subdirectory(resource_chunk)
add_subdirectory(resource_unit_loader)

add_library(WBE_LIB_RESOURCE STATIC
    ${wbe_resource_src}
)

set(libs ${libs}
    WBE_LIB_RESOURCE
    PARENT_SCOPE
)

