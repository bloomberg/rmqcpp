add_executable(librmq_cancelconsumer
    librmq_cancelconsumer.m.cpp
)

target_link_libraries(librmq_cancelconsumer PUBLIC
    bsl
    bal
    bdl
    rmq
    rmqintegration
)

add_custom_target(test_integration_cancelconsumer
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_cancelconsumer>
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_cancelconsumer
         )

