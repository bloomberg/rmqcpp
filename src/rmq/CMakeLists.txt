add_subdirectory(rmqt)
add_subdirectory(rmqamqpt)
add_subdirectory(rmqio)
add_subdirectory(rmqp)
add_subdirectory(rmqamqp)
add_subdirectory(rmqa)

add_library(rmq STATIC
    $<TARGET_OBJECTS:rmqamqpt>
    $<TARGET_OBJECTS:rmqt>
    $<TARGET_OBJECTS:rmqio>
    $<TARGET_OBJECTS:rmqp>
    $<TARGET_OBJECTS:rmqamqp>
    $<TARGET_OBJECTS:rmqa>
)

target_link_libraries(rmq PUBLIC bsl bdl bal ZLIB::ZLIB OpenSSL::Crypto OpenSSL::SSL)

get_target_property(OPENSSL_TARGET_TYPE OpenSSL::SSL TYPE)
if(OPENSSL_CRYPTO_LIBRARY MATCHES "\\.a$")
    target_compile_definitions(rmq PUBLIC WITH_STATIC_LIB_libcrypto)
endif()

target_compile_definitions(rmq PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_include_directories(rmq PUBLIC $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rmqt>)
target_include_directories(rmq PUBLIC $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rmqp>)
target_include_directories(rmq PUBLIC $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rmqa>)


file(GLOB RMQ_PUBLIC_HEADERS rmqa/*h rmqp/*h rmqt/*h)
set_target_properties(rmq PROPERTIES PUBLIC_HEADER "${RMQ_PUBLIC_HEADERS}")

install(
    TARGETS rmq
    EXPORT rmqcppTargets
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT librmq-dev
    PUBLIC_HEADER
        DESTINATION include
        COMPONENT librmq-dev
)

install(EXPORT rmqcppTargets 
        FILE rmqcppTargets.cmake 
        DESTINATION share/rmqcpp 
        NAMESPACE rmqcpp::)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in 
                              "${CMAKE_CURRENT_BINARY_DIR}/rmqcppConfig.cmake"
                              INSTALL_DESTINATION share/rmqcpp) 
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rmqcppConfig.cmake" DESTINATION share/rmqcpp)

# Emit some metadata required internally
set(RMQ_PC_DEP_NAMES bsl bdl bal openssl)
find_package(GenBDEMetadata QUIET)
if (GenBDEMetadata_FOUND)
    gen_bde_metadata(PACKAGE_GROUP rmq INSTALL_COMPONENT librmq-dev DEPS "${RMQ_PC_DEP_NAMES}")
endif()
find_package(EmitPkgConfigFile QUIET)
if (EmitPkgConfigFile_FOUND)
    emit_pkgconfig_file(PKG rmq INSTALL_COMPONENT librmq-dev DEPS "${RMQ_PC_DEP_NAMES}")
endif()
