if (WBE_BUILD_PLATFORM STREQUAL "Linux")
    add_subdirectory(linux)
else()
    message(FATAL_ERROR "Platform: ${WBE_BUILD_PLATFORM} not supported for white bird engine.")
endif()

