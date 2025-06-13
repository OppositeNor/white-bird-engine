file(GLOB wbe_ecs_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_ecs_test
    ${wbe_ecs_test_src}
)

target_link_libraries(wbe_ecs_test
    ${libs}
    gtest_main
)
