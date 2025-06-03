file(GLOB logic_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(LOGIC_LIB STATIC
    ${logic_src}
)

set(libs ${libs}
    LOGIC_LIB
    PARENT_SCOPE
)


