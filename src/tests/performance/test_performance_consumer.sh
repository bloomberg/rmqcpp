#!/bin/bash

: "${RABBITMQ_PRODUCER:=tests/integration/producer/librmq_producer}"
: "${RABBITMQ_CONSUMER:=tests/integration/consumemessages/librmq_consumemessages}"
: "${RMQ_HOSTNAME:=localhost}"
: "${RMQ_PORT:=5672}"
: "${RMQ_VHOST:='rmqcpp'}"
: "${RMQ_MGMT:='localhost:15672'}"
: "${RABBITMQ_MESSAGE_SIZE:=100}"
: "${RABBITMQ_QOS:=100}"
: "${TEST_N:=50000}"
: "${RMQ_TRACING:=}"
: "${TRACING:-without}"

if [ "$TRACING" == "with" ]; then
    RMQ_TRACING="-d"
fi

id=$(openssl rand -base64 6)

uri="amqp://${RMQ_VHOST}:${RMQ_VHOST}@${RMQ_HOSTNAME}:${RMQ_PORT}/${RMQ_VHOST}"

"${RMQ_PRODUCER}" "${RMQ_TRACING}" --uri "${uri}" -q "${id}" -w 0 -l "${RABBITMQ_QOS}" -o ERROR -n "${TEST_N}" --messageSize "${RABBITMQ_MESSAGE_SIZE}" --expires 60000 > /dev/null

echo "Running the consumer"
"${RMQ_CONSUMER}" "${RMQ_TRACING}" --uri "${uri}" -q "${id}" -l "${RABBITMQ_QOS}" -o ERROR -n "${TEST_N}" --expires 60000

curl -su "${RMQ_VHOST}:${RMQ_VHOST}" -H "content-type:application/json" \
    -XDELETE "http://${RMQ_MGMT}/api/queues/${RMQ_VHOST}/${id}"

exit 0
