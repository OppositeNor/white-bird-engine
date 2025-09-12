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
if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_subdirectory(linux)
elseif(WBE_BUILD_PLATFORM STREQUAL "Windows")
    add_subdirectory(windows)
elseif(WBE_BUILD_PLATFORM STREQUAL "Darwin")
    add_subdirectory(macos)
else()
    message(FATAL_ERROR "Platform: ${WBE_BUILD_PLATFORM} not supported for white bird engine.")
endif()

set(WBE_TEST_EXE ${WBE_TEST_EXE}
    PARENT_SCOPE
)

