add_custom_target(test_integration_rmqapitests
         COMMAND
            RMQ_EXE=$<TARGET_FILE:rmqperftest> 
            ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS rmqperftest
         COMMAND
            RMQ_EXE=$<TARGET_FILE:rmqperftest> 
            RMQPERFTEST_SHUFFLE_CONNECTION_ENDPOINTS=1 ${PYTEST} ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}/test_shuffle_connect.py
         DEPENDS rmqperftest
         )
