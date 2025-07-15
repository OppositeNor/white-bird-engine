cmake_minimum_required(VERSION 3.18)

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
file(MAKE_DIRECTORY ${WBE_RESOURCE_DIR})
set(WBE_RESOURCE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)
file(MAKE_DIRECTORY ${WBE_RESOURCE_OUTPUT_DIR})

set(WBE_SHADER_DIR ${WBE_RESOURCE_DIR}/shaders)
file(MAKE_DIRECTORY ${WBE_SHADER_DIR})
set(WBE_SHADER_OUTPUT_DIR ${WBE_RESOURCE_OUTPUT_DIR}/shaders)
file(MAKE_DIRECTORY ${WBE_SHADER_OUTPUT_DIR})
set(WBE_METADATA_OUTPUT_PATH ${WBE_RESOURCE_OUTPUT_DIR}/metadata.json)

set(WBE_TEST_ENV_DIR ${PROJECT_SOURCE_DIR}/test_env)
set(WBE_TEST_ENV_RES_DIR ${WBE_TEST_ENV_DIR}/res)

set(WBE_BUILD_CONFIG_SCRIPTS "")

# Add dependencies subdirectories
if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android dependencies
else()
    set(SDL_TEST OFF)
    set(SDL_STATIC ON)
    set(SDL_SHARED OFF)
    add_subdirectory(dependencies/SDL2)
endif()

set(YAML_BUILD_SHARED_LIBS OFF)
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
add_subdirectory(dependencies/yaml-cpp)
set(libs yaml-cpp::yaml-cpp)

# Set C++ build flags
if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pedantic-errors")
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    ## for optimization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
else()
    ## for gdb or valgrind
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fno-inline -fno-omit-frame-pointer -D_DEBUG")
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
    set(include_dirs ${include_dirs} ${PROJECT_SOURCE_DIR}/dependencies/SDL2)
endif()
include_directories(
    ${include_dirs}
)

# Add engine source
add_subdirectory(src/resource)
add_subdirectory(src/core)
add_subdirectory(src/platform)

# Create executable
if (WBE_BUILD_PLATFORM STREQUAL "Android")
    # TODO: Android build
else()
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCE_DIR}/src/main.cpp
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC
        SDL2::SDL2-static
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

message("Build config scripts: ${WBE_BUILD_CONFIG_SCRIPTS}")

# Runtime reflections
add_custom_target(wbe_on_build
    COMMENT "Running White Bird Engine build script"
    COMMAND python3 ${PROJECT_SOURCE_DIR}/on_build_scripts/on_build.py ${WBE_METADATA_OUTPUT_PATH} ${WBE_BUILD_CONFIG_SCRIPTS}
)

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
        COMMAND python3 ${PROJECT_SOURCE_DIR}/on_build_scripts/run_tests.py ${WBE_TEST_EXE}
    )

    add_custom_target(wbe_setup_test_env
        COMMENT "Setting up test_env..."
        COMMAND ${CMAKE_COMMAND} -E copy
            "${WBE_METADATA_OUTPUT_PATH}"
            "${WBE_TEST_ENV_RES_DIR}"
    )

endif()

add_dependencies(${PROJECT_NAME} wbe_on_build)
add_dependencies(${PROJECT_NAME} compile_shaders)

if (WBE_MAKE_TEST)
    add_dependencies(wbe_run_tests wbe_setup_test_env)
endif()
