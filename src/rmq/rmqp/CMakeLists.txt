add_library(rmqp OBJECT
    rmqp_consumer.cpp
    rmqp_consumertracing.cpp
    rmqp_connection.cpp
    rmqp_messageguard.cpp
    rmqp_metricpublisher.cpp
    rmqp_producer.cpp
    rmqp_producertracing.cpp
    rmqp_rabbitcontext.cpp
    rmqp_topology.cpp
    rmqp_topologyupdate.cpp)

target_link_libraries(rmqp PUBLIC
    bsl
    rmqt
    rmqio)

target_include_directories(rmqp PUBLIC .)
