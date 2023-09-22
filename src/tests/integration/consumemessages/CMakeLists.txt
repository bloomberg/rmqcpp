add_executable(librmq_consumemessages 
    librmq_consumemessages.m.cpp
)

target_link_libraries(librmq_consumemessages PUBLIC 
    bsl
    bal
    rmq
    rmqtestutil
    rmqintegration
)

install(
    TARGETS librmq_consumemessages
    DESTINATION bin
    COMPONENT rmqtools)
