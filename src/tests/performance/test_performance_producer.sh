#!/bin/bash

: "${RMQ_PRODUCER:='tests/integration/producer/librmq_producer'}"
: "${RMQ_HOSTNAME:='localhost'}"
: "${RMQ_PORT:=5672}"
: "${RMQ_VHOST:='rmq-lib'}"
: "${RMQ_MGMT:='localhost:15672'}"
: "${TEST_N:=50000}"
: "${RABBITMQ_QOS:=100}"
: "${RABBITMQ_MESSAGE_SIZE:=1000}"
: "${ENABLE_PERF:=false}"
: "${TRACING:-'without'}"
: "${RMQ_TRACING:=}"

if [ "$TRACING" == "with" ]; then
    RMQ_TRACING="-d"
fi

id=$(openssl rand -base64 6)

if $ENABLE_PERF
then
    echo "Running the producer (with perf record)..."
    ${RMQ_PRODUCER} ${RMQ_TRACING} -v ${RMQ_VHOST} -u ${RMQ_VHOST} -s ${RMQ_VHOST} -q "test_producer_performance_${id}" -o ERROR -n ${TEST_N} -w 0 -l ${RABBITMQ_QOS} -p ${RMQ_PORT} --messageSize ${RABBITMQ_MESSAGE_SIZE} ${RMQ_HOSTNAME} &
    TASK_PID=$!
    echo -e "\nEstablishing connection..."
    sleep 3
    echo -e "\nRecording performance of the task using perf tool for 15 seconds..."
    perf record -F 99 -p $TASK_PID --call-graph dwarf  -- sleep 15
    echo -e "\nWaiting for the producer to complete..."
    wait $TASK_PID
else
    echo "Running the producer with args: ${RMQ_PRODUCER} ${RMQ_TRACING} -v ${RMQ_VHOST} -u ${RMQ_VHOST} -s ${RMQ_VHOST} -q "test_producer_performance_${id}" -o ERROR -n ${TEST_N} -w 0 -l ${RABBITMQ_QOS} -p ${RMQ_PORT} --messageSize ${RABBITMQ_MESSAGE_SIZE} --expires 60000 --messageTTL 5 ${RMQ_HOSTNAME}"
    ${RMQ_PRODUCER} ${RMQ_TRACING} -v ${RMQ_VHOST} -u ${RMQ_VHOST} -s ${RMQ_VHOST} -q "test_producer_performance_${id}" -o ERROR -n ${TEST_N} -w 0 -l ${RABBITMQ_QOS} -p ${RMQ_PORT} --messageSize ${RABBITMQ_MESSAGE_SIZE} --expires 60000 --messageTTL 5 ${RMQ_HOSTNAME}
fi

exit 0
