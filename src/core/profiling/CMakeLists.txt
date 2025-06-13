file(GLOB wbe_profiling_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_core_src
    ${wbe_core_src}
    ${wbe_profiling_src}
    PARENT_SCOPE
)

