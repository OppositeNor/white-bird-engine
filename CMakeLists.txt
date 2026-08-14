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

project(WhiteBirdEngine LANGUAGES C CXX)

# Use C++ 23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(WBE_MAKE_TEST "Export test for White Bird Engine" ON)
set(WBE_BINARY_DIR ${CMAKE_BINARY_DIR}/bin CACHE STRING "The binary output directory.")
option(WBE_BUILD_SHARED "Should build shared library" ON)

# Output pathes
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${WBE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${WBE_BINARY_DIR}/libs)

set(WBE_BUILD_PLATFORM ${CMAKE_SYSTEM_NAME} CACHE STRING "Build platform.")

if(WBE_BUILD_SHARED)
    set(WBE_LIB_BUILD_TYPE SHARED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
else()
    set(WBE_LIB_BUILD_TYPE STATIC)
endif()

# Add dependencies subdirectories
if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android dependencies
else()
    set(SDL_TEST OFF)
    set(SDL_STATIC OFF)
    set(SDL_SHARED ON)
    add_subdirectory(dependencies/SDL2)
endif()
find_package(Vulkan REQUIRED)
set(VMA_ENABLE_INSTALL OFF)
set(VMA_BUILD_DOCUMENTATION OFF)
set(VMA_BUILD_SAMPLES OFF)
add_subdirectory(dependencies/VulkanMemoryAllocator)

set(YAML_BUILD_SHARED_LIBS OFF)
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
add_subdirectory(dependencies/yaml-cpp)
set(BUILD_TESTING OFF)
add_subdirectory(dependencies/boost)
if (WBE_MAKE_TEST)
    set(BENCHMARK_ENABLE_TESTING OFF)
    add_compile_options(-Wno-c2y-extensions)
    add_subdirectory(dependencies/googletest)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument")
    add_subdirectory(dependencies/google-benchmark)
endif()
# Setup miniaudio
add_library(WBE_MINIAUDIO_BUILD_LIB
    dependencies/miniaudio/miniaudio.c
)


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
endif()

if (WBE_MAKE_TEST AND CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
    message("Enabling coverage tests.")
    add_compile_options(--coverage -Wno-stringop-truncation)
    add_link_options(--coverage)
endif()

# Platform specific configurations.
if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_compile_options(-DWBE_TARGET_PLATFORM_LINUX)
    if (WBE_BUILD_SHARED)
        set(WBE_USE_RPATH_LIBRARY ON)
    endif()
elseif(WBE_BUILD_PLATFORM STREQUAL "Windows")
    add_compile_options(-DWBE_TARGET_PLATFORM_WINDOWS)
elseif(WBE_BUILD_PLATFORM STREQUAL "Darwin")
    add_compile_options(-DWBE_TARGET_PLATFORM_MACOS)
    if (WBE_BUILD_SHARED)
        set(WBE_USE_RPATH_LIBRARY ON)
    endif()
endif()


if (WBE_MAKE_TEST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWBE_BUILD_TEST=OFF")
endif()

# Use default aligned gentypes in glm
add_definitions(-DGLM_FORCE_DEFAULT_ALIGNED_GENTYPES)

# Set include directories
set(include_dirs 
    ${Vulkan_INCLUDE_DIR}
    ${WBE_INCLUDE_DIR}
    ${WBE_PER_TARGET_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/dependencies/entt/single_include
    ${PROJECT_SOURCE_DIR}/dependencies/glm
    ${PROJECT_SOURCE_DIR}/dependencies/json/single_include
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp/include
    ${PROJECT_SOURCE_DIR}/dependencies/stb
    ${PROJECT_SOURCE_DIR}/dependencies/boost-headers
    ${PROJECT_SOURCE_DIR}/dependencies/miniaudio
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
    GPUOpen::VulkanMemoryAllocator
    SDL2::SDL2
    yaml-cpp::yaml-cpp
    boost_thread
    boost_program_options
    WBE_MINIAUDIO_BUILD_LIB
)

# Add engine source
set(WBE_SOURCES "")
add_subdirectory(src/platform)
add_subdirectory(src/core)
add_subdirectory(src/resource)
add_subdirectory(src/function)
add_subdirectory(src/global)
add_subdirectory(src/generated)

add_library(WBE_ENGINE ${WBE_LIB_BUILD_TYPE}
    ${WBE_SOURCES}
)
add_library(WBE::Engine ALIAS WBE_ENGINE)

target_link_libraries(WBE_ENGINE PUBLIC
    ${libs}
)

set(libs ${libs}
    WBE_ENGINE
)

# Create executable
if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android build
else()
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCE_DIR}/src/main.cpp
    )
    if (WBE_USE_RPATH_LIBRARY)
        set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/libs"
        SKIP_BUILD_RPATH FALSE
        )
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        WBE_ENGINE
    )
endif()

# Generate test executables
if (WBE_MAKE_TEST)
    # Export tests
    add_subdirectory(tests)
endif()
