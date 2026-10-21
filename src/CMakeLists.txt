find_package(Threads REQUIRED) # CMake 3.26-rc3 Bug https://gitlab.kitware.com/cmake/cmake/-/issues/24505
find_package(ZLIB REQUIRED)
if (ENABLE_COMPRESSION)
    find_package(zstd CONFIG REQUIRED)
endif()
find_package(Boost REQUIRED)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
if(rmqcpp_ENABLE_TESTING)
    find_package(GTest REQUIRED)
endif()

# BDE-required subpackages
find_package(bal REQUIRED)

add_subdirectory(rmq)
if(rmqcpp_ENABLE_TESTING)
    add_subdirectory(rmqtestmocks)
    add_subdirectory(tests)
endif()
