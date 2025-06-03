file(GLOB animation_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(ANIMATION_LIB STATIC
    ${animation_src}
)

set(libs ${libs}
    ANIMATION_LIB
    PARENT_SCOPE
)



