if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro" )
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    add_compile_options(_RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")
endif()

add_subdirectory(rmqtestutil)
add_subdirectory(integration)
add_subdirectory(performance)
add_subdirectory(rmqamqp)
add_subdirectory(rmqamqpt)
add_subdirectory(rmqio)
add_subdirectory(rmqt)
add_subdirectory(rmqa)
add_subdirectory(rmqtestmocks)


add_custom_target(ctest COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS 
rmqamqp_tests
rmqamqpt_tests
rmqio_tests
rmqt_tests
rmqtestutil_tests
rmqa_tests
)
