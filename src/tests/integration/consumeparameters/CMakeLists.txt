add_executable(librmq_consumeparameters 
    librmq_consumeparameters.m.cpp
)

target_link_libraries(librmq_consumeparameters PUBLIC 
    bsl
    bal
    bdl
    rmq
    rmqintegration
)

add_custom_target(test_integration_consumeparameters
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_consumeparameters>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_consumeparameters
         )

install(
    TARGETS librmq_consumeparameters
    DESTINATION bin
    COMPONENT rmqtools)

