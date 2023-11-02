cmake_minimum_required(VERSION 3.25) 
project(rmq LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
enable_testing()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(src)
add_subdirectory(examples)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "SunPro")
    # _RWSTD_ALLOCATOR tells the solaris <memory> header to define a std::allocator
    # which conforms better to the C++ standard, which is expected by Boost. Without
    # this, the library does not build due to missing std::allocator<..>::rebind

    # "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__" tells the solaris sys/socket.h to conform
    # better to what boost asio expects (to have a msg_flags member)
    target_compile_definitions(rmq PRIVATE _RWSTD_ALLOCATOR "-D_XOPEN_SOURCE=500" "-D__EXTENSIONS__")

    # Solaris doesn't pull these symbols in automatically
    # Libraries such as bte set this manually (we don't depend on bte)
    target_link_options(rmq PUBLIC -lnsl -lsocket)

    set(PKG_LIBS "-lnsl -lsocket")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "XL")
    # The ibm/xl compiler does not appreciate this boost feature
    target_compile_definitions(rmq PRIVATE BOOST_ASIO_DISABLE_HANDLER_TYPE_REQUIREMENTS)
else()
    # In general the sunpro/xl warnings are not worth the bother
    set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
endif()
