# Heartbeats

In RabbitMQ, the heartbeat timeout is negotiated between the client
and the server, defaulting to 60 seconds on the server side.  The
client can negotiate a smaller heartbeat time than the server
suggests, but not a larger one.  It can also veto heartbeats
completely.

`rmqcpp` should in future allow users to reduce the heartbeat interval
in the lower level `rmqb` library, but will not allow vetoing heartbeats
in any circumstances because heartbeats are useful to detect silently
dropped TCP connections.

Heartbeats are sent at half the overall heartbeat interval, so the
client will be disconnected after two heartbeats were missed.

## Heartbeat Frame

The heartbeat frame in RabbitMQ is an empty frame with frame type 8.
The channel and the frame length are always 0.  So a full heartbeat
frame will always be `0x800000CE`.

## RabbitMQ <3.7.11 heartbeat bug

Prior to RabbitMQ 3.7.11 the first heartbeat took twice as long to be sent
by the broker. rmqcpp works around this by giving the first heartbeat twice
as long to arrive.

https://github.com/rabbitmq/rabbitmq-common/pull/293/files
