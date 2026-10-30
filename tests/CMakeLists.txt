add_executable(
    avmanager_tests
    google_example_test.cpp
    avr_handler_test.cpp
)
set_target_properties(avmanager_tests PROPERTIES OUTPUT_NAME AVManager_TestSuite)
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)
gtest_discover_tests(avmanager_tests)
target_link_libraries(avmanager_tests PRIVATE avmanager_core GTest::gtest_main)
if(MSVC)
    target_compile_options(avmanager_tests PRIVATE /W4)
endif()
