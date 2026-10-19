set(RMQAPITESTS_ARGS
    -o python_files=*_tests.py
    --pyargs integration.rmqapitests
)

# TODO: enable TestConsumerPauseResume
# TODO: enable hosthealthawareness
add_custom_target(test_integration_rmqapitests
         COMMAND
            RMQ_EXE=$<TARGET_FILE:rmqperftest>
            ${PYTEST} ${PYTEST_ARGS} ${RMQAPITESTS_ARGS}
            --capability connection consumer producer topology
            -k "not TestConsumerPauseResume"
         COMMAND
            RMQ_EXE=$<TARGET_FILE:rmqperftest>
            RMQPERFTEST_SHUFFLE_CONNECTION_ENDPOINTS=1
            ${PYTEST} ${PYTEST_ARGS} ${RMQAPITESTS_ARGS}
            --capability connection
            -k TestRmqConnects
         DEPENDS rmqperftest
         VERBATIM
         )
