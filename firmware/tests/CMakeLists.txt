file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *_test.cpp)
add_executable(cubey_tests main.cpp fake_hal.cpp ${TEST_SOURCES})
target_link_libraries(cubey_tests PRIVATE cubey_core doctest::doctest)
add_test(NAME cubey_tests COMMAND cubey_tests)
