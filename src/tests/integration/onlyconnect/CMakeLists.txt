add_executable(librmq_onlyconnect 
    librmq_onlyconnect.m.cpp
)


 # rmqamqp dep required as onlyconnect uses an internal API to avoid creating consumers/producers
target_link_libraries(librmq_onlyconnect PUBLIC 
    rmqamqp
    $<TARGET_PROPERTY:rmqamqp,LINK_LIBRARIES>
    rmq
    rmqintegration
)

add_custom_target(test_integration_onlyconnect
         COMMAND 
            RMQ_EXE=$<TARGET_FILE:librmq_onlyconnect> 
            python3.8 -m pytest ${PYTEST_ARGS} ${CMAKE_CURRENT_LIST_DIR}
         DEPENDS librmq_onlyconnect 
         )
         
install(
    TARGETS librmq_onlyconnect
    DESTINATION bin
    COMPONENT rmqtools)
