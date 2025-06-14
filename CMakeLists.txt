cmake_minimum_required(VERSION 3.18)

project(WhiteBirdEngine)

file(GLOB source ${PROJECT_SOURCE_DIR}/src/*.cpp)

set(SDL_TEST OFF)
set(SDL_STATIC ON)
set(SDL_SHARED OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WBE_RESOURCE_DIR ${PROJECT_SOURCE_DIR}/res)
file(MAKE_DIRECTORY ${WBE_RESOURCE_DIR})
set(WBE_RESOURCE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)
file(MAKE_DIRECTORY ${WBE_RESOURCE_OUTPUT_DIR})

set(WBE_SHADER_DIR ${WBE_RESOURCE_DIR}/shaders)
file(MAKE_DIRECTORY ${WBE_SHADER_DIR})
set(WBE_SHADER_OUTPUT_DIR ${WBE_RESOURCE_OUTPUT_DIR}/shaders)
file(MAKE_DIRECTORY ${WBE_SHADER_OUTPUT_DIR})

set(WBE_METADATA_OUTPUT_DIR ${WBE_RESOURCE_OUTPUT_DIR}/metadata)
file(MAKE_DIRECTORY ${WBE_METADATA_OUTPUT_DIR})

option(WBE_MAKE_TEST "Export test for White Bird Engine" ON)
set(WBE_BUILD_PLATFORM "Linux" CACHE STRING "Build platform.")

set(WBE_BUILD_CONFIG_SCRIPTS "")

add_subdirectory(dependencies/SDL2)
add_subdirectory(dependencies/glm)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -pedantic-errors")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    ## for optimization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
else()
    ## for gdb or valgrind
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fno-inline -fno-omit-frame-pointer -D_DEBUG")
endif()

set(libs "")
set(include_dirs 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/dependencies/glm
)

if (WBE_MAKE_TEST)
    # Export tests
    add_subdirectory(dependencies/googletest)
    set(include_dirs
        ${include_dirs}
        ${PROJECT_SOURCE_DIR}/dependencies/googletest/googletest
    )
endif()

include_directories(
    ${include_dirs}
)

add_subdirectory(src/core)
add_subdirectory(src/platform)

add_executable(${PROJECT_NAME}
    ${source}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    SDL2
    Vulkan::Vulkan
    ${libs}
)

if (WBE_MAKE_TEST)
    add_subdirectory(tests)
endif()

message("Build config scripts: ${WBE_BUILD_CONFIG_SCRIPTS}")

# runtime reflections
add_custom_target(wbe_build_script
    COMMENT "Running White Bird Engine build script"
    COMMAND python ${PROJECT_SOURCE_DIR}/build_scripts/build.py ${WBE_METADATA_OUTPUT_DIR} ${WBE_BUILD_CONFIG_SCRIPTS}
)
add_dependencies(${PROJECT_NAME} wbe_build_script)

# compile shaders
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
add_dependencies(${PROJECT_NAME} compile_shaders)
