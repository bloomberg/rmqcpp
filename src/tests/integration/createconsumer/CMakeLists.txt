add_executable(librmq_createconsumer 
    librmq_createconsumer.m.cpp
)

target_link_libraries(librmq_createconsumer PUBLIC 
    bsl
    bal
    bdl
    rmq
    rmqintegration
)

add_custom_target(test_integration_consumer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_createconsumer>
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_createconsumer
         )
         
install(
    TARGETS librmq_createconsumer
    DESTINATION bin
    COMPONENT rmqtools)
