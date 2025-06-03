file(GLOB render_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(view_src
    ${view_src}
    ${render_src}
    PARENT_SCOPE
)
