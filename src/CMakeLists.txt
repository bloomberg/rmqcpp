find_package(Threads REQUIRED) # CMake 3.26-rc3 Bug https://gitlab.kitware.com/cmake/cmake/-/issues/24505
find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set(BDE_PACKAGE_SEARCH_PATH "share/bde;lib64/cmake")
else()
    set(BDE_PACKAGE_SEARCH_PATH "share/bde;lib/cmake")
endif()

# BDE-required subpackages TODO
find_package(ryu REQUIRED CONFIG PATH_SUFFIXES ${BDE_PACKAGE_SEARCH_PATH})
find_package(pcre2 REQUIRED CONFIG PATH_SUFFIXES ${BDE_PACKAGE_SEARCH_PATH})
find_package(inteldfp REQUIRED CONFIG PATH_SUFFIXES ${BDE_PACKAGE_SEARCH_PATH})

find_package(bsl REQUIRED CONFIG PATH_SUFFIXES ${BDE_PACKAGE_SEARCH_PATH})
find_package(bdl REQUIRED CONFIG PATH_SUFFIXES ${BDE_PACKAGE_SEARCH_PATH})
find_package(bal REQUIRED CONFIG PATH_SUFFIXES ${BDE_PACKAGE_SEARCH_PATH})

# BDE-required subpackages
#find_package(bsl REQUIRED)
#find_package(bdl REQUIRED)
#find_package(bal REQUIRED)

add_subdirectory(rmq)
add_subdirectory(rmqtestmocks)
add_subdirectory(tests)
