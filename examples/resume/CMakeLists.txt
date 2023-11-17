find_package(bsl REQUIRED)

add_executable(resume_consumer
    resume.m.cpp
)

target_link_libraries(resume_consumer PUBLIC
    rmq
    bsl
)
