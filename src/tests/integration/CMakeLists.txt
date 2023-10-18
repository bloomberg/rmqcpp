add_library(rmqintegration OBJECT
    rmqintegration_testparameters.cpp
)

target_link_libraries(rmqintegration PUBLIC
    bsl
    bdl
    bal
    rmq
)

target_include_directories(rmqintegration PUBLIC .)

find_package (Python3 COMPONENTS Interpreter REQUIRED)
execute_process (COMMAND "${Python3_EXECUTABLE}" -m venv ${CMAKE_CURRENT_BINARY_DIR}/.venv)

set (ENV{VIRTUAL_ENV} ${CMAKE_CURRENT_BINARY_DIR}/.venv)

execute_process(COMMAND ${CMAKE_CURRENT_BINARY_DIR}/.venv/bin/pip install  -r ${CMAKE_CURRENT_LIST_DIR}/requirements.txt)

set(PYTEST
    ${CMAKE_CURRENT_BINARY_DIR}/.venv/bin/python -m pytest
)
set( PYTEST_ARGS "-q" "-rap" "--maxfail=2" "--tb=short" "--durations=1" "--log-level=error" "-s")


add_subdirectory(rmqperftest)
add_subdirectory(consumemessages)
add_subdirectory(producer)
add_subdirectory(multithread_producer)
add_subdirectory(multithread_consumer)
add_subdirectory(exitboth)
add_subdirectory(queuedelete)

add_custom_target(test_integration
    DEPENDS
        test_integration_multithread_producer
        test_integration_multithread_consumer
        test_integration_exit_both
        test_integration_rmqapitests
        test_queue_delete
)
