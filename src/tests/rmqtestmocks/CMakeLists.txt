add_executable(rmqtestmocks_tests
    rmqtestmocks.m.cpp
    rmqtestmocks_mocks.t.cpp
)

target_link_libraries(rmqtestmocks_tests PUBLIC 
    rmqtestmocks
    GTest::gtest
    GTest::gmock
)

target_include_directories(rmqtestmocks_tests PRIVATE .)

add_test(NAME rmqtestmocks_tests COMMAND rmqtestmocks_tests)
