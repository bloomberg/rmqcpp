#!/bin/bash

: "${RABBITMQ_PRODUCER:='tests/integration/producer/librmq_producer'}"
: "${RABBITMQ_CONSUMER:='tests/integration/consumemessages/librmq_consumemessages'}"
: "${RMQ_HOSTNAME:='localhost'}"
: "${RMQ_PORT:=5672}"
: "${RMQ_VHOST:='rmqcpp'}"
: "${RMQ_MGMT:='localhost:15672'}"
: "${RABBITMQ_MESSAGE_SIZE:=100}"
: "${RABBITMQ_QOS:=100}"
: "${TEST_N:=50000}"
: "${RMQ_TRACING:=}"
: "${TRACING:-'without'}"

if [ "$TRACING" == "with" ]; then
    RMQ_TRACING="-d"
fi

id=$(openssl rand -base64 6)

${RMQ_PRODUCER} ${RMQ_TRACING} -v ${RMQ_VHOST} -u ${RMQ_VHOST} -s ${RMQ_VHOST} -q ${id} -w 0 -l ${RABBITMQ_QOS} -o ERROR -n ${TEST_N} --messageSize ${RABBITMQ_MESSAGE_SIZE} -p ${RMQ_PORT} --expires 60000 ${RMQ_HOSTNAME} > /dev/null

echo "Running the consumer"
${RMQ_CONSUMER} ${RMQ_TRACING} -v ${RMQ_VHOST} -u ${RMQ_VHOST} -s ${RMQ_VHOST} -q ${id} -l ${RABBITMQ_QOS} -o ERROR -n ${TEST_N} -p ${RMQ_PORT} --expires 60000 ${RMQ_HOSTNAME}

curl -su ${RMQ_VHOST}:${RMQ_VHOST} -H "content-type:application/json" \
    -XDELETE http://${RMQ_MGMT}/api/queues/${RMQ_VHOST}/${id}

exit 0
