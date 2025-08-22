set(TEST_NAME wbe_serializer_json_test)

file(GLOB ${TEST_NAME}_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${TEST_NAME}
    ${${TEST_NAME}_src}
)

target_link_libraries(${TEST_NAME}
    ${libs}
    gtest_main
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

