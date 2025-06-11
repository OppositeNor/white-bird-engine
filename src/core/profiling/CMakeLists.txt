file(GLOB profiling_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(PROFILING_LIB STATIC
    ${profiling_src}
)

set(libs ${libs}
    PROFILING_LIB
    PARENT_SCOPE
)


