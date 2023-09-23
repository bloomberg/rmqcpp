add_executable(librmq_exitboth 
    librmq_exitboth.m.cpp
)

target_compile_definitions(librmq_exitboth PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_link_libraries(librmq_exitboth PUBLIC 
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_integration_exit_both
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_exitboth>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_exitboth
         )

