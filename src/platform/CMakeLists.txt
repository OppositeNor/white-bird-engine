set(wbe_platform_src "")

if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_subdirectory(linux)
elseif(WBE_BUILD_PLATFORM STREQUAL "Windows")
    add_subdirectory(windows)
elseif(WBE_BUILD_PLATFORM STREQUAL "Darwin")
    add_subdirectory(macos)
else()
    message(FATAL_ERROR "Platform: ${WBE_BUILD_PLATFORM} not supported for white bird engine.")
endif()

add_subdirectory(file_system)

add_library(WBE_LIB_PLATFORM STATIC
    ${wbe_platform_src}
)

set(libs ${libs}
    WBE_LIB_PLATFORM
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)
