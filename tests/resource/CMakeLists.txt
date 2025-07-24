add_subdirectory(resource_chunk_test)
add_subdirectory(resource_loader_test)
add_subdirectory(resource_utils_test)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

if(WBE_BUILD_PLATFORM STREQUAL "Windows")
    set(WBE_TEST_EXE ${WBE_TEST_EXE}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.exe
        PARENT_SCOPE
    )
else()
    set(WBE_TEST_EXE ${WBE_TEST_EXE}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}
        PARENT_SCOPE
    )
endif()

