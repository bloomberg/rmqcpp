# Event Loop

There is one main event loop in the `rmqcpp` library per RabbitContext.
It drives the I/O, sending messages to the broker, and receiving them and
heartbeats.  It is owned by the `RabbitContext`, and gets events from and hands
them back to `rmqa`.

## `rmqio`

The `rmqio` event loop will essentially have three parts for the
initial version of the library.

- timers: used to deal with the heartbeats.

- read from network: read data from the network.  The data is passed
  to `rmqamqp` via a callback, and dispatched further to client
  queues, threadpools or producers.
