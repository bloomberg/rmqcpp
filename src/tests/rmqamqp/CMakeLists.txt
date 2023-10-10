add_executable(rmqamqp_tests
    rmqamqp.m.cpp
    rmqamqp_channel.t.cpp
    rmqamqp_channelmap.t.cpp
    rmqamqp_channeltests.t.cpp
    rmqamqp_connection.t.cpp
    rmqamqp_contentmaker.t.cpp
    rmqamqp_framer.t.cpp
    rmqamqp_heartbeatmanagerimpl.t.cpp
    rmqamqp_messagestore.t.cpp
    rmqamqp_multipleackhandler.t.cpp
    rmqamqp_receivechannel.t.cpp
    rmqamqp_sendchannel.t.cpp
    rmqamqp_topologytransformer.t.cpp
    rmqamqp_topologymerger.t.cpp
)

target_link_libraries(rmqamqp_tests PUBLIC
    rmqtestutil
    rmqamqp
    rmqt
    $<TARGET_PROPERTY:rmqamqp,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqt,LINK_LIBRARIES>
    GTest::gtest
    GTest::gmock
)
target_include_directories(rmqamqp_tests PRIVATE .)

add_test(NAME rmqamqp_tests COMMAND rmqamqp_tests)
