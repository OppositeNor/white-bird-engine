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

set(WBE_SHADER_DIR ${WBE_RESOURCE_DIR}/shaders)
set(WBE_SHADER_OUTPUT_DIR ${WBE_RESOURCE_OUTPUT_DIR}/shaders)
set(WBE_METADATA_OUTPUT_PATH ${WBE_RESOURCE_OUTPUT_DIR}/metadata.json)

set(WBE_CONFIG_DIR ${WBE_RESOURCE_DIR}/config)
set(WBE_CONFIG_OUTPUT_DIR ${WBE_RESOURCE_OUTPUT_DIR}/config)

set(WBE_METADATA_CACHE_DIR ${CMAKE_CURRENT_BINARY_DIR}/metadata_cache)

set(WBE_TEST_ENV_DIR ${PROJECT_SOURCE_DIR}/test_env)
set(WBE_TEST_ENV_RES_DIR ${WBE_TEST_ENV_DIR}/res)

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

find_package(Boost REQUIRED CONFIG PATHS dependencies/boost/tools/boost_install)

# Set C++ build flags
if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pedantic-errors")
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Deploy")
    ## for optimization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -flto -DNDEBUG")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    ## for optimization and testing
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -flto -g -DNDEBUG")
else()
    ## for gdb or valgrind
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fno-inline -fno-omit-frame-pointer -D_DEBUG")
endif()

if (WBE_MAKE_TEST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWBE_BUILD_TEST=OFF")
endif()

# Set include directories
set(include_dirs 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/dependencies/glm
    ${PROJECT_SOURCE_DIR}/dependencies/json/single_include
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp/include
    ${PROJECT_SOURCE_DIR}/dependencies/stb
)
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
set(WBE_TEST_EXE "")
if (WBE_MAKE_TEST)
    # Export tests
    add_subdirectory(dependencies/googletest)
    set(include_dirs
        ${include_dirs}
        ${PROJECT_SOURCE_DIR}/dependencies/googletest/googletest
    )
    add_subdirectory(tests)
endif()

# Compile shaders
file(GLOB shader_sources ${WBE_SHADER_DIR}/*.vert ${WBE_SHADER_DIR}/*.frag ${WBE_SHADER_DIR}/*.comp)
set(shaders_outputs "")
foreach(shader_source ${shader_sources})
    message("Compiling: ${shader_source}")
    get_filename_component(shader_name ${shader_source} NAME)
    set(output_file "${WBE_SHADER_OUTPUT_DIR}/${shader_name}.spv")

    add_custom_command(
        OUTPUT ${output_file}
        COMMAND glslc ${shader_source} -o ${output_file}
        DEPENDS ${shader_source}
        VERBATIM
    )

    list(APPEND shaders_outputs ${output_file})
endforeach()

add_custom_target(compile_shaders DEPENDS ${shaders_outputs})

# Run all tests
if (WBE_MAKE_TEST)
    add_custom_target(wbe_run_tests
        COMMENT "Running White Bird Engine tests..."
        COMMAND python3 ${PROJECT_SOURCE_DIR}/test_script/run_tests.py ${WBE_TEST_EXE}
    )

    add_custom_target(wbe_setup_test_env
        COMMENT "Setting up test_env..."
        COMMAND ${CMAKE_COMMAND} -E copy
            "${WBE_METADATA_OUTPUT_PATH}"
            "${WBE_TEST_ENV_RES_DIR}"
    )

endif()

add_dependencies(${PROJECT_NAME} compile_shaders)

if (WBE_MAKE_TEST)
    add_dependencies(wbe_run_tests wbe_setup_test_env)
endif()
