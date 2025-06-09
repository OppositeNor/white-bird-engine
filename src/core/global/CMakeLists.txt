file(GLOB global_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(GLOBAL_LIB STATIC
    ${global_src}
)

set(libs ${libs}
    GLOBAL_LIB
    PARENT_SCOPE
)

