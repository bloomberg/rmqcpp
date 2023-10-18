#!/usr/bin/bash

set -e

if [ -z "${VHOST}" ]; then
    echo "VHOST not specified!"
    exit 1
fi

sed -i "s/{VHOST}/${VHOST}/g" /etc/rabbitmq/rabbitmq.conf
echo "Starting rabbitmq server"
rabbitmq-server
