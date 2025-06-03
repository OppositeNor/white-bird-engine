file(GLOB view_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_subdirectory(renderer)

add_library(VIEW_LIB STATIC
    ${view_src}
)

set(libs ${libs}
    VIEW_LIB
    PARENT_SCOPE
)

