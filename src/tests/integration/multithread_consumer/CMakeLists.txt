add_executable(librmq_multithread_consumer 
    librmq_multithread_consumer.m.cpp
)

target_link_libraries(librmq_multithread_consumer PUBLIC 
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_multithread_consumer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_multithread_consumer>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_multithread_consumer
         )
