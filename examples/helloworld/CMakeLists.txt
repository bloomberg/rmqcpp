find_package(bsl REQUIRED)

add_executable(rmqhelloworld_producer
    producer.m.cpp
)

target_link_libraries(rmqhelloworld_producer PUBLIC
    rmq
    bsl
)
