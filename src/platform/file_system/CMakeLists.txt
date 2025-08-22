file(GLOB wbe_platform_file_system_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_platform_src
    ${wbe_platform_src}
    ${wbe_platform_file_system_src}
    PARENT_SCOPE
)


