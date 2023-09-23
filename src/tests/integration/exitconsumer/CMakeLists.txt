add_executable(librmq_exitconsumer 
    librmq_exitconsumer.m.cpp
)

target_compile_definitions(librmq_exitconsumer PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_link_libraries(librmq_exitconsumer PUBLIC 
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_exit_consumer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_exitconsumer>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_exitconsumer
         )
