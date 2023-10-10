find_package(bsl REQUIRED)

add_executable(rmqtopology_headers
    headers.m.cpp
)

target_link_libraries(rmqtopology_headers PUBLIC
    rmq
    bsl
)
