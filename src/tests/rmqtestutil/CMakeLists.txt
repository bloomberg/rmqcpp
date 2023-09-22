add_library(rmqtestutil
    rmqtestutil.m.cpp
    rmqtestutil_callcount.cpp
    rmqtestutil_mockchannel.t.cpp
    rmqtestutil_mockeventloop.t.cpp
    rmqtestutil_mockmetricpublisher.cpp
    rmqtestutil_mockresolver.t.cpp
    rmqtestutil_mockretryhandler.t.cpp
    rmqtestutil_mocktimerfactory.cpp
    rmqtestutil_replayframe.cpp
    rmqtestutil_savethreadid.cpp
    rmqtestutil_timedmetric.cpp
    rmqtestutil_timeoverride.cpp
)

target_link_libraries(rmqtestutil PUBLIC 
    bsl
    bdl
    rmqamqpt
    rmqamqp
    rmqt
    $<TARGET_PROPERTY:rmqamqpt,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqamqp,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqt,LINK_LIBRARIES>
    GTest::gmock
    GTest::gtest
)

target_include_directories(rmqtestutil PUBLIC .)

add_executable(rmqtestutil_tests
    rmqtestutil_replayframe.t.cpp
)

target_link_libraries(rmqtestutil_tests PUBLIC
    rmqtestutil
    rmqamqpt
    rmqamqp
    $<TARGET_PROPERTY:rmqamqpt,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqamqp,LINK_LIBRARIES>
    GTest::gtest
    GTest::gmock
)

add_test(NAME rmqtestutil_tests COMMAND rmqtestutil_tests)
