add_executable(librmq_queuedelete
    librmq_queuedelete.m.cpp
)

target_compile_definitions(librmq_queuedelete PRIVATE USES_LIBRMQ_EXPERIMENTAL_FEATURES)

target_link_libraries(librmq_queuedelete PUBLIC 
    bsl
    bal
    rmq
    rmqintegration
)

add_custom_target(test_queue_delete
         COMMAND
            RMQ_EXE=$<TARGET_FILE:librmq_queuedelete>
            RMQ_ALT_EXE=$<TARGET_FILE:rmqperftest>
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_queuedelete
         )

install(
    TARGETS librmq_queuedelete
    DESTINATION bin
    COMPONENT rmqtools)
