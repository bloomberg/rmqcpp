add_executable(rmqa_tests
    rmqa.m.cpp
    rmqa_consumerimpl.t.cpp
    rmqa_connectionimpl.t.cpp
    rmqa_connectionstring.t.cpp
    rmqa_messageguard.t.cpp
    rmqa_producerimpl.t.cpp
    rmqa_rabbitcontextimpl.t.cpp
    rmqa_rabbitcontextoptions.t.cpp
    rmqa_topology.t.cpp
    rmqa_vhostimpl.t.cpp
    rmqa_connectionmonitor.t.cpp
)

target_link_libraries(rmqa_tests PUBLIC
    rmqtestutil
    rmqa
    rmqt
    rmqio
    rmqtestmocks
    $<TARGET_PROPERTY:rmqa,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqt,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqio,LINK_LIBRARIES>
    GTest::gtest
    GTest::gmock
)

add_test(NAME rmqa_tests COMMAND rmqa_tests)
