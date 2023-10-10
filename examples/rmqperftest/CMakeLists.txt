find_package(bsl REQUIRED)
find_package(bal REQUIRED)

add_executable(rmqperftest
    rmqperftest.m.cpp
    rmqperftest_args.cpp
    rmqperftest_consumerargs.cpp
    rmqperftest_runner.cpp
)

target_compile_definitions(rmqperftest PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_link_libraries(rmqperftest PUBLIC
    bsl
    bal
    rmq
    rmqintegration
)
target_include_directories(rmqperftest PRIVATE .)

install(
    TARGETS rmqperftest
    DESTINATION bin
    COMPONENT rmqtools
)
