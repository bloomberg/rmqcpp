add_library(rmqio OBJECT 
    rmqio_asioconnection.cpp
    rmqio_asioeventloop.cpp
    rmqio_asioresolver.cpp
    rmqio_asiotimer.cpp
    rmqio_backofflevelretrystrategy.cpp
    rmqio_connection.cpp
    rmqio_connectionretryhandler.cpp
    rmqio_decoder.cpp
    rmqio_eventloop.cpp
    rmqio_resolver.cpp
    rmqio_retryhandler.cpp
    rmqio_retrystrategy.cpp
    rmqio_serializedframe.cpp
    rmqio_task.cpp
    rmqio_watchdog.cpp
)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(rmqio PUBLIC
    ZLIB::ZLIB
    OpenSSL::Crypto
    OpenSSL::SSL
    bsl
    bdl
    bal
    rmqamqpt
    rmqt
)

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro" )
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    target_compile_definitions(rmqio PRIVATE _RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")

    target_link_options(rmqio PUBLIC -lnsl -lsocket)
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "XL")
    target_compile_definitions(rmqio PRIVATE BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS)
endif()
target_include_directories(rmqio PUBLIC .)
