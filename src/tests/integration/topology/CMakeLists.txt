add_executable(librmq_topology 
    librmq_topology.m.cpp
)

target_link_libraries(librmq_topology PUBLIC
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_topology
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_topology>
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_topology
         )


install(
    TARGETS librmq_topology
    DESTINATION bin
    COMPONENT rmqtools)
