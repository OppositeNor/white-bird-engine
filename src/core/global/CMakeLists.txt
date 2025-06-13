file(GLOB wbe_global_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_core_src
    ${wbe_core_src}
    ${wbe_global_src}
    PARENT_SCOPE
)
