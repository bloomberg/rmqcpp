add_executable(rmqio_tests
    rmqio.m.cpp
    rmqio_asioeventloop.t.cpp
    rmqio_asiotimer.t.cpp
    rmqio_asioconnection.t.cpp
    rmqio_asioresolver.t.cpp
    rmqio_backofflevelretrystrategy.t.cpp
    rmqio_connectionretryhandler.t.cpp
    rmqio_decoder.t.cpp
    rmqio_eventloop.t.cpp
    rmqio_retryhandler.t.cpp
    rmqio_watchdog.t.cpp
)

target_link_libraries(rmqio_tests PUBLIC
    rmqamqpt
    rmqt
    rmqio
    rmqamqp
    $<TARGET_PROPERTY:rmqt,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqamqpt,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqio,LINK_LIBRARIES>
    $<TARGET_PROPERTY:rmqamqp,LINK_LIBRARIES>
    rmqtestutil
    GTest::gtest
    GTest::gmock
)

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro" )
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    target_compile_definitions(rmqio_tests PRIVATE _RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "XL")
    target_compile_definitions(rmqio_tests PRIVATE BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS)
endif()

add_test(NAME rmqio_tests COMMAND rmqio_tests)
