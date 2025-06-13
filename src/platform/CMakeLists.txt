set(wbe_platform_src "")
if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_subdirectory(linux)
else()
    message(FATAL_ERROR "Platform: ${WBE_BUILD_PLATFORM} not supported for white bird engine.")
endif()

add_library(WBE_LIB_PLATFORM STATIC
    ${wbe_platform_src}
)

set(libs ${libs}
    WBE_LIB_PLATFORM
    PARENT_SCOPE
)
