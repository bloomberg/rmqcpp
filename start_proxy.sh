#!/usr/bin/bash

set -e

if [ -z "${VHOST}" ]; then
    echo "VHOST not specified!"
    exit 1
fi

if [ -e /tmp/amqpprox ]; then
    echo "Cleaning up /tmp/amqpprox from previous run"
    rm  /tmp/amqpprox || echo "rm /tmp/amqpprox failed"
fi

echo "Starting proxy"
/opt/bb/bin/amqpprox &
# wait until the control socket file exists before setting config
while ! [ -e /tmp/amqpprox ]
do
    sleep 1
done

/opt/bb/bin/amqpprox_ctl /tmp/amqpprox backend add rabbit DC_1 rabbit 5672
/opt/bb/bin/amqpprox_ctl /tmp/amqpprox map backend "${VHOST}" rabbit
/opt/bb/bin/amqpprox_ctl /tmp/amqpprox listen start 30424
# /opt/bb/bin/amqpprox_ctl /tmp/amqpprox TLS INGRESS KEY_FILE /certs/server-key.pem
# /opt/bb/bin/amqpprox_ctl /tmp/amqpprox TLS INGRESS CERT_CHAIN_FILE /certs/server.crt
# /opt/bb/bin/amqpprox_ctl /tmp/amqpprox TLS INGRESS CA_CERT_FILE /certs/ca.crt
# /opt/bb/bin/amqpprox_ctl /tmp/amqpprox listen start_secure 5671
socat TCP-LISTEN:30454,fork UNIX-CONNECT:/tmp/amqpprox
