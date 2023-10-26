find_package(Threads REQUIRED) # CMake 3.26-rc3 Bug https://gitlab.kitware.com/cmake/cmake/-/issues/24505
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)

# BDE-required subpackages
find_package(bal REQUIRED)

add_subdirectory(rmq)
add_subdirectory(rmqtestmocks)
add_subdirectory(tests)
