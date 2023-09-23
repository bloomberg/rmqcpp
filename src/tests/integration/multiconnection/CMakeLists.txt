add_executable(librmq_multiconnection
        librmq_multiconnection.m.cpp
)

target_link_libraries(librmq_multiconnection PUBLIC
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_multiconnection
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_multiconnection>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_multiconnection
         )

install(
    TARGETS librmq_multiconnection
    DESTINATION bin
    COMPONENT rmqtools)
