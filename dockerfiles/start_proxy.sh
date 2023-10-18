#!/usr/bin/bash

set -e

AMQPPROX_DIR="${AMQPPROX_DIR:-/usr/local}"
AMQPPROX_SOCK="${AMQPPROX_SOCK:-/tmp/amqpprox}"


if [ -z "${VHOST}" ]; then
    echo "VHOST not specified!"
    exit 1
fi

if [ -S "${AMQPPROX_SOCK}" ]; then
    echo "Cleaning up ${AMQPPROX_SOCK} from previous run"
    rm  "${AMQPPROX_SOCK}" || echo "rm ${AMQPPROX_SOCK} failed"
fi

echo "Starting proxy"
"${AMQPPROX_DIR}"/bin/amqpprox &

# wait until the control socket file exists before setting config
while ! [ -S "${AMQPPROX_SOCK}" ]
do
    sleep 1
done

"${AMQPPROX_DIR}"/bin/amqpprox_ctl /tmp/amqpprox log console 3 #info
"${AMQPPROX_DIR}"/bin/amqpprox_ctl /tmp/amqpprox datacenter set DC_1 
"${AMQPPROX_DIR}"/bin/amqpprox_ctl /tmp/amqpprox backend add rabbit DC_1 rabbit 5672 
"${AMQPPROX_DIR}"/bin/amqpprox_ctl /tmp/amqpprox map backend "${VHOST}" rabbit
"${AMQPPROX_DIR}"/bin/amqpprox_ctl /tmp/amqpprox listen start 5672
socat TCP-LISTEN:5700,fork UNIX-CONNECT:"${AMQPPROX_SOCK}"
