
add_library(rmqtestmocks
    rmqtestmocks_mockconsumer.cpp
    rmqtestmocks_mockconnection.cpp
    rmqtestmocks_mockmessageguard.cpp
    rmqtestmocks_mockrabbitcontext.cpp
    rmqtestmocks_mockproducer.cpp
    rmqtestmocks_mockvhost.cpp
)

target_link_libraries(rmqtestmocks PUBLIC
    bsl
    bdl
    rmq
    GTest::gmock
)

target_include_directories(rmqtestmocks PUBLIC .)

file(GLOB RMQTESTMOCKS_PUBLIC_HEADERS *h)
set_target_properties(rmqtestmocks PROPERTIES PUBLIC_HEADER "${RMQTESTMOCKS_PUBLIC_HEADERS}")

install(
    TARGETS rmqtestmocks
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT librmqtestmocks-dev
    PUBLIC_HEADER
        DESTINATION include
        COMPONENT librmqtestmocks-dev
)

# Emit some metadata required internally
set(RMQ_PC_DEP_NAMES bsl bdl rmq gmock)
find_package(GenBDEMetadata QUIET)
if (GenBDEMetadata_FOUND)
    gen_bde_metadata(PACKAGE rmqtestmocks INSTALL_COMPONENT librmqtestmocks-dev DEPS "${RMQ_PC_DEP_NAMES}")
endif()
find_package(EmitPkgConfigFile QUIET)
if (EmitPkgConfigFile_FOUND)
    emit_pkgconfig_file(PKG rmqtestmocks INSTALL_COMPONENT librmqtestmocks-dev DEPS "${RMQ_PC_DEP_NAMES}")
endif()
