add_executable(librmq_producer 
    librmq_producer.m.cpp
)

target_link_libraries(librmq_producer PUBLIC 
    bsl
    bal
    rmq
    rmqtestutil
    rmqintegration
)

add_custom_target(test_integration_producer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_producer>
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_producer
         )

install(
    TARGETS librmq_producer
    DESTINATION bin
    COMPONENT rmqtools)