file(GLOB wbe_utils_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_core_src
    ${wbe_core_src}
    ${wbe_utils_src}
    PARENT_SCOPE
)
