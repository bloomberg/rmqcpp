add_executable(rmqt_tests
    rmqt.m.cpp
    rmqt_consumerconfig.t.cpp
    rmqt_envelope.t.cpp
    rmqt_exchange.t.cpp
    rmqt_fieldvalue.t.cpp
    rmqt_future.t.cpp
    rmqt_message.t.cpp
    rmqt_plaincredentials.t.cpp
    rmqt_secureendpoint.t.cpp
    rmqt_simpleendpoint.t.cpp
)

target_link_libraries(rmqt_tests PUBLIC 
    rmqt
    $<TARGET_PROPERTY:rmqt,LINK_LIBRARIES>
    GTest::gtest
    GTest::gmock
)

target_include_directories(rmqt_tests PRIVATE .)

add_test(NAME rmqt_tests COMMAND rmqt_tests)
