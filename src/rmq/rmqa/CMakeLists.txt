add_library(rmqa OBJECT 
    rmqa_consumer.cpp
    rmqa_consumerimpl.cpp
    rmqa_connectionimpl.cpp
    rmqa_connectionstring.cpp
    rmqa_connectionmonitor.cpp
    rmqa_messageguard.cpp
    rmqa_noopmetricpublisher.cpp
    rmqa_producer.cpp
    rmqa_producerimpl.cpp
    rmqa_rabbitcontext.cpp
    rmqa_rabbitcontextimpl.cpp
    rmqa_rabbitcontextoptions.cpp
    rmqa_topology.cpp
    rmqa_topologyupdate.cpp
    rmqa_tracingconsumerimpl.cpp
    rmqa_tracingmessageguard.cpp
    rmqa_tracingproducerimpl.cpp
    rmqa_vhost.cpp
    rmqa_vhostimpl.cpp
)


target_link_libraries(rmqa PUBLIC
    bsl
    bdl
    bal
    rmqt 
    rmqp
    rmqamqp
    rmqio
    rmqamqpt
)
if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro" )
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    target_compile_definitions(rmqa PRIVATE _RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")
endif()


target_compile_definitions(rmqa PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)
target_include_directories(rmqa PUBLIC .)
