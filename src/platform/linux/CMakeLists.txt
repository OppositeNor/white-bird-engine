file(GLOB wbe_linux_platform_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_platform_src
    ${wbe_platform_src}
    ${wbe_linux_platform_src}
    PARENT_SCOPE
)

