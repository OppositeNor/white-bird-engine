# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_subdirectory(linux)
elseif(WBE_BUILD_PLATFORM STREQUAL "Windows")
    add_subdirectory(windows)
elseif(WBE_BUILD_PLATFORM STREQUAL "Darwin")
    add_subdirectory(macos)
else()
    message(FATAL_ERROR "Platform: ${WBE_BUILD_PLATFORM} not supported for white bird engine.")
endif()

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

set(WBE_TEST_EXE ${WBE_TEST_EXE}
    PARENT_SCOPE
)
