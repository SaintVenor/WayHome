add_executable(unit_tests tests.cpp)

target_link_libraries(unit_tests 
    PRIVATE 
        yandex_api_lib
        GTest::gtest
        GTest::gmock
)

target_compile_definitions(unit_tests PRIVATE 
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(unit_tests PRIVATE ws2_32 crypt32)
endif()

include(GoogleTest)
gtest_discover_tests(unit_tests)