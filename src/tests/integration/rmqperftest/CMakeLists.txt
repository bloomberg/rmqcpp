add_custom_target(test_integration_rmqapitests
         COMMAND
            RMQ_VHOST=rmq-lib
            RMQ_EXE=$<TARGET_FILE:rmqperftest> 
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS rmqperftest
         COMMAND
            RMQ_VHOST=rmq-lib
            RMQ_EXE=$<TARGET_FILE:rmqperftest> 
            RMQPERFTEST_SHUFFLE_CONNECTION_ENDPOINTS=1 python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}/test_shuffle_connect.py
         DEPENDS rmqperftest
         )
