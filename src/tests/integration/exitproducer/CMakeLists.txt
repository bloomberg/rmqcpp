add_executable(librmq_exitproducer 
    librmq_exitproducer.m.cpp
)

target_compile_definitions(librmq_exitproducer PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_link_libraries(librmq_exitproducer PUBLIC 
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_exit_producer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_exitproducer>
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_exitproducer
         )

