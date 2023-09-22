add_library(rmqintegration OBJECT
    rmqintegration_testparameters.cpp
)

target_link_libraries(rmqintegration PUBLIC
    bsl
    bdl
    bal
    rmq
)

target_include_directories(rmqintegration PUBLIC .)

set( PYTEST_ARGS "-q" "-ra" "--maxfail=2" "--tb=short" "--durations=1" "--log-level=debug" "-s")


add_subdirectory(rmqperftest)
add_subdirectory(consumemessages)
add_subdirectory(consumeparameters)
add_subdirectory(createconsumer)
add_subdirectory(cancelconsumer)
add_subdirectory(onlyconnect)
add_subdirectory(multithread_producer)
add_subdirectory(multithread_consumer)
add_subdirectory(multiconnection)
add_subdirectory(topology)
add_subdirectory(exitproducer)
add_subdirectory(exitconsumer)
add_subdirectory(exitboth)
add_subdirectory(queuedelete)
add_subdirectory(producer)

add_custom_target(test_integration
    DEPENDS
        test_integration_multithread_producer
        test_integration_multithread_consumer
        test_integration_exit_both
        test_integration_rmqapitests
        test_queue_delete
)
