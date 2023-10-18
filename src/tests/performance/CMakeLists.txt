add_custom_target(test_performance_producer
	COMMAND
		RMQ_PRODUCER=$<TARGET_FILE:librmq_producer>
		bash ${CMAKE_CURRENT_LIST_DIR}/test_performance_producer.sh
	DEPENDS librmq_producer)

add_custom_target(test_performance_consumer
	COMMAND
		RMQ_CONSUMER=$<TARGET_FILE:librmq_consumemessages>
		RMQ_PRODUCER=$<TARGET_FILE:librmq_producer>
		bash ${CMAKE_CURRENT_LIST_DIR}/test_performance_consumer.sh
	DEPENDS librmq_consumemessages librmq_producer)

add_custom_target(test_performance
    DEPENDS
	test_performance_producer
	test_performance_consumer
)
