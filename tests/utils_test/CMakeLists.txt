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
set(TEST_NAME wbe_utils_test)

# ---

file(GLOB ${TEST_NAME}_headers ${CMAKE_CURRENT_SOURCE_DIR}/*.hh)
file(GLOB ${TEST_NAME}_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${TEST_NAME}
    ${${TEST_NAME}_src}
)

target_link_libraries(${TEST_NAME}
    ${libs}
    gtest_main
)

if(WBE_BUILD_PLATFORM STREQUAL "Windows")
    set(WBE_TEST_EXE ${WBE_TEST_EXE}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.exe
        PARENT_SCOPE
    )
else()
    set(WBE_TEST_EXE ${WBE_TEST_EXE}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}
        PARENT_SCOPE
    )
endif()

