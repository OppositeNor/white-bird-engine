file(GLOB wbe_resource_chunk_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_resource_src
    ${wbe_resource_src}
    ${wbe_resource_chunk_src}
    PARENT_SCOPE
)

