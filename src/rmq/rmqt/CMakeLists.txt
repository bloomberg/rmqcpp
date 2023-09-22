add_library(rmqt OBJECT
    rmqt_binding.cpp
    rmqt_confirmresponse.cpp
    rmqt_consumerack.cpp
    rmqt_consumerackbatch.cpp
    rmqt_consumerconfig.cpp
    rmqt_credentials.cpp
    rmqt_endpoint.cpp
    rmqt_envelope.cpp
    rmqt_exchange.cpp
    rmqt_exchangebinding.cpp
    rmqt_exchangetype.cpp
    rmqt_fieldvalue.cpp
    rmqt_future.cpp
    rmqt_message.cpp
    rmqt_mutualsecurityparameters.cpp
    rmqt_plaincredentials.cpp
    rmqt_properties.cpp
    rmqt_queue.cpp
    rmqt_queuebinding.cpp
    rmqt_queuedelete.cpp
    rmqt_queueunbinding.cpp
    rmqt_result.cpp
    rmqt_secureendpoint.cpp
    rmqt_securityparameters.cpp
    rmqt_shortstring.cpp
    rmqt_simpleendpoint.cpp
    rmqt_topology.cpp
    rmqt_topologyupdate.cpp
    rmqt_vhostinfo.cpp
    )

target_link_libraries(rmqt PUBLIC
    bsl
    bdl
    bal
)

target_include_directories(rmqt PUBLIC .)
