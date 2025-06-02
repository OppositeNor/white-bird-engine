file(GLOB allocator_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(ALLOCATOR_LIB STATIC
    ${allocator_src}
)

set(libs ${libs}
    ALLOCATOR_LIB
    PARENT_SCOPE
)

