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
cmake_minimum_required(VERSION 3.20)

project(WhiteBirdEngine)

# Options
option(WBE_MAKE_TEST "Export test for White Bird Engine" ON)
set(WBE_BUILD_PLATFORM ${CMAKE_SYSTEM_NAME} CACHE STRING "Build platform.")

# Use C++ 20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define output paths
set(WBE_RESOURCE_DIR ${PROJECT_SOURCE_DIR}/res)
set(WBE_RESOURCE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)

set(WBE_CONFIG_DIR ${WBE_RESOURCE_DIR}/config)
set(WBE_CONFIG_OUTPUT_DIR ${WBE_RESOURCE_OUTPUT_DIR}/config)

set(WBE_METADATA_CACHE_DIR ${CMAKE_CURRENT_BINARY_DIR}/metadata_cache)

set(WBE_TEST_ENV_DIR ${PROJECT_SOURCE_DIR}/test_env)

# Add dependencies subdirectories
if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android dependencies
else()
    set(SDL_TEST OFF)
    set(SDL_STATIC ON)
    set(SDL_SHARED OFF)
    add_subdirectory(dependencies/SDL2)
endif()
set(BUILD_TESTING OFF)
find_package(Vulkan REQUIRED)

set(YAML_BUILD_SHARED_LIBS OFF)
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
add_subdirectory(dependencies/yaml-cpp)
set(BUILD_TESTING OFF)
add_subdirectory(dependencies/boost)
if (WBE_MAKE_TEST)
    add_subdirectory(dependencies/googletest)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument")
    add_subdirectory(dependencies/google-benchmark)
endif()


# Set C++ build flags
if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pedantic-errors")
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Deploy")
    # for optimization
    add_compile_options(-O2 -DNDEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    # for optimization and testing
    add_compile_options(-O2 -g -DNDEBUG)
else()
    # for gdb or valgrind
    add_compile_options(-O0 -g -fno-inline -fno-omit-frame-pointer -D_DEBUG)
    add_link_options(--coverage)
endif()

# Platform declerations
if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_compile_options(-DWBE_TARGET_PLATFORM_LINUX)
elseif(WBE_BUILD_PLATFORM STREQUAL "Windows")
    add_compile_options(-DWBE_TARGET_PLATFORM_WINDOWS)
elseif(WBE_BUILD_PLATFORM STREQUAL "Darwin")
    add_compile_options(-DWBE_TARGET_PLATFORM_MACOS)
endif()


if (WBE_MAKE_TEST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWBE_BUILD_TEST=OFF")
endif()

# Set include directories
set(include_dirs 
    ${Vulkan_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/dependencies/glm
    ${PROJECT_SOURCE_DIR}/dependencies/json/single_include
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp/include
    ${PROJECT_SOURCE_DIR}/dependencies/stb
    ${PROJECT_SOURCE_DIR}/dependencies/boost-headers
)
if (WBE_MAKE_TEST)
    set(include_dirs
        ${include_dirs}
        ${PROJECT_SOURCE_DIR}/dependencies/googletest/googletest
        ${PROJECT_SOURCE_DIR}/dependencies/google-benchmark/include
    )
endif()

if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android include directories
else()
    set(include_dirs ${include_dirs} ${PROJECT_SOURCE_DIR}/dependencies/SDL2/include)
endif()

include_directories(
    ${include_dirs}
)

set(libs ${libs}
    Vulkan::Vulkan
    SDL2::SDL2-static
    yaml-cpp::yaml-cpp
    boost_thread
)

# Add engine source
set(WBE_SOURCES "")
add_subdirectory(src/platform)
add_subdirectory(src/core)
add_subdirectory(src/resource)
add_subdirectory(src/function)
add_subdirectory(src/global)

add_library(WBE_LIB
    ${WBE_SOURCES}
)

target_link_libraries(WBE_LIB INTERFACE
    ${libs}
)

set(libs ${libs}
    WBE_LIB
)

# Create executable
if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android build
else()
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCE_DIR}/src/main.cpp
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${libs}
    )
endif()

# Generate test executables
if (WBE_MAKE_TEST)
    # Export tests
    add_subdirectory(tests)
endif()


