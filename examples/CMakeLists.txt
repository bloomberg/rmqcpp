add_subdirectory(rmqperftest)

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro") AND NOT (CMAKE_CXX_COMPILER_ID STREQUAL "XL"))
    # The examples don't need to work on these platforms
    add_subdirectory(helloworld)
    add_subdirectory(topology)
endif()
