file(GLOB linux_platform_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(PLATFORM_LIB STATIC
    ${linux_platform_src}
)

set(libs ${libs}
    PLATFORM_LIB
    PARENT_SCOPE
)




