add_executable(librmq_multithread_producer 
    librmq_multithread_producer.m.cpp
)

target_link_libraries(librmq_multithread_producer PUBLIC 
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_multithread_producer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_multithread_producer>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_multithread_producer
         )
