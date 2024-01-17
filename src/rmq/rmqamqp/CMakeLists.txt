add_library(rmqamqp OBJECT
    rmqamqp_channel.cpp
    rmqamqp_channelcontainer.cpp
    rmqamqp_channelfactory.cpp
    rmqamqp_channelmap.cpp
    rmqamqp_connection.cpp
    rmqamqp_connectionmonitor.cpp
    rmqamqp_contentmaker.cpp
    rmqamqp_framer.cpp
    rmqamqp_heartbeatmanager.cpp
    rmqamqp_heartbeatmanagerimpl.cpp
    rmqamqp_message.cpp
    rmqamqp_messagestore.cpp
    rmqamqp_messagewithroute.cpp
    rmqamqp_metrics.cpp
    rmqamqp_multipleackhandler.cpp
    rmqamqp_receivechannel.cpp
    rmqamqp_sendchannel.cpp
    rmqamqp_topologytransformer.cpp
    rmqamqp_topologymerger.cpp)

target_link_libraries(rmqamqp PUBLIC
    bsl
    bdl
    bal
    rmqio
    rmqamqpt
    rmqp
    rmqt
)

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro" )
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    target_compile_definitions(rmqamqp PRIVATE _RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")
endif()
target_include_directories(rmqamqp PUBLIC .)
