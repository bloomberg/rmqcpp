# rmqcpp

`rmqcpp` is a RabbitMQ client library written in C++03.

## RabbitContext

`rmqa::RabbitContext` provides an API for connecting to RabbitMQ virtual hosts (vhosts). The `RabbitContext` object stores:

- Threadpool for callbacks;
- Metric publisher; and
- Error callback.

`RabbitContext` contains default implementations of them as detailed below. Users can replace these components with a `rmqa::RabbitContextOptions` object.

Threadpool is used for asynchronous callbacks to the user code (e.g., received messages, publisher confirms, error callbacks). A `bdlmt::ThreadPool` is created by default.

Metric publisher is used to publish internal library metrics. Users can provide their own implementation for publishing these metrics by implementing the `rmqp::MetricPublisher` interface. Alternatively, publishing can be disabled by passing an instance of `rmqa::NoOpMetricPublisher`.

Error callback is called when a connection or channel is closed by the RabbitMQ broker.

Important -- `RabbitContext` object must outlive other library objects (`VHost`, `Producer`, `Consumer`).

```cpp

// Create a RabbitContext object. This object must outlive all other library objects.
rmqa::RabbitContext context;

// Only if necessary, create an options object to provide custom components for the library
rmqa::RabbitContextOptions options;
options.setThreadpool(/* threadpool */);
options.setMetricPublisher(/* metric publisher */);
options.setErrorCallback(/* error callback */);

rmqa::RabbitContext contextWithOptions(options);

```


## VHost

`rmqa::VHost` provides an API for creating producer and consumer objects on the selected RabbitMQ vhost. A `VHost` object can be created from `rmqa::RabbitContext`. 

```cpp
rmqt::VHostInfo vhostInfo(
    bsl::make_shared<rmqt::SimpleEndpoint>("rabbit", "vhost-name"),
    bsl::make_shared<rmqt::PlainCredentials>(guest, guest));
    
bsl::shared_ptr<rmqa::VHost> vhost = context.createVHostConnection(
    "my-producer-connection", vhostInfo); // returns immediately
```

Creating a `rmqa::VHost` instance **does not** immediately create a connection with the RabbitMQ broker. These connections are created lazily when calling `rmqa::VHost::createProducer` and `rmqa::VHost::createConsumer`.


## Topology

For a more elaborate documentation of the topology, see [AMQP 0-9-1 Model Explained](https://www.rabbitmq.com/tutorials/amqp-concepts.html).

AMQP topology consists of:
- exchanges;
- queues; and
- bindings.

Each publisher sends messages to a particular exchange. Each consumer consumes messages from a particular queue. Exchanges and queues are connected using bindings.

```cpp

// Create topology
rmqa::Topology topology;
rmqt::QueueHandle q1    = topology.addQueue("queue-name");
rmqt::ExchangeHandle e1 = topology.addExchange("exch-name");

// Bind e1 and q1 using binding key 'key'
topology.bind(e1, q1, "key1");

// To create an auto-generated queue
rmqt::QueueHandle q2 = topology.addQueue();
topology.bind(e1, q2, "key2");

```

## Producing messages

Messages can be sent to a RabbitMQ broker using a `rmqa::Producer` object. To create one, it is necessary to provide:
- a topology object;
- the exchange to which the publisher will publish; and
- the maximum number of unconfirmed messages before `send` blocks.

```cpp
// How many messages can be awaiting confirmation before `send` blocks
const uint16_t maxOutstandingConfirms = 10;

// Create a producer object
rmqt::Result<rmqa::Producer> producerResult =
    vhost->createProducer(topology, e1, maxOutstandingConfirms);

if (!producerResult) {
    // Handle error
    bsl::cout << "Error creating connection: " << producerResult.error();
    return -1;
}

bsl::shared_ptr<rmqa::Producer> producer = producerResult.value();
```

When sending a message, provide a callback function to be invoked when the publisher confirm is received from the broker.

```cpp
void receiveConfirmation(const rmqt::Message& message,
                         const bsl::string& routingKey,
                         const rmqt::ConfirmResponse& response)
{
    if (response.status() == rmqt::ConfirmResponse::ACK) {
        // Message is now guaranteed to be safe with the broker
    }
    else {
        // REJECT / RETURN indicate problem with the send request (bad routing
        // key?)
        // NB: users must handle the send failure to avoid dropping the message!
        BALL_LOG_WARN << "Sending message failed: " << response;
    }
}
```

To send a message, construct a `rmqt::Message` object. Each message needs its own object even when intentionally sending the same message multiple times -- this is because the message ID must be unique.

```cpp
// Work in progress -- the interface for constructing message payloads is still subject to change
bsl::string messageText    = "Hello RabbitMQ!";
bsl::vector<uint8_t> > payload(messageText.cbegin(), messageText.cend());
rmqt::Message message(bsl::make_shared<bsl::vector<uint8_t> >(payload));

// Method `send` returns immediately unless there are `maxOutstandingConfirms` unacknowledged
// messages already, in which case it waits until at least one confirm comes back.
// User must wait until the confirm callback is executed before considering
// the send to be committed.
const rmqp::Producer::SendStatus sendResult =
    producer->send(message, "key", &receiveConfirmation);

if (sendResult != rmqp::Producer::SENDING) {
    // handle errors
    return -1;
}

// Blocks until `timeout` expires or all confirmations have been received
// Note this could block forever if a separate thread continues publishing
if (!producer->waitForConfirms(/* timeout */)) {
    // Timeout expired
}
```

When shutting down the producer, it is necessary to wait until it receives all outstanding publisher confirms to avoid losing the unconfirmed messages.

```cpp
// Blocks until `timeout` expires or all confirmations have been received
// Note this could block forever if a separate thread continues publishing
if (!producer->waitForConfirms(/* timeout */)) {
    // Timeout expired
}
```


## Consumer

Messages can be consumed from a RabbitMQ broker using a `rmqa::Consumer`.

Messages are consumed asynchronously. The user-provided consumer callback method will be invoked on the `RabbitContext` threadpool for every message received.

A `Consumer` object is created with the following arguments:
- the topology that will be declared to the broker;
- the queue that the consumer will consume from (must be part of `topology`);
- consumer callback: invoked on every message received (an example is provided below);
- consumer label: useful for identifying the consumer in the RabbitMQ Management UI; and
- prefetch count: the maximum number of unacknowledged messages before the broker will wait for acks.

```cpp

rmqt::Result<rmqa::Consumer> consumerResult = 
    vhost->createConsumer(
        topology,            // topology
        q1,                  // queue
        MessageConsumer(),   // consumer callback
        "my consumer label", // Consumer Label (shows in Management UI)
        5                    // prefetch count
    );

```

Here is an example of a consumer callback implementation:

```cpp

// Consumer callback
class MessageConsumer {
  public:
    void operator()(rmqp::MessageGuard& guard)
    {
        BALL_LOG_INFO << "Received message: " << guard.message() << " Content: "
                      << bsl::string((const char*)guard.message().payload(),
                                     guard.message().payloadSize());
        guard.ack();
    }
};

```

**Important** -- messages should be acknowledged only after completely processing them. For example, if the consumer logic sends received messages to some RabbitMQ exchange, the message should be acknowledged only after receiving the publisher confirm of the send operation. A premature call to `ack()` followed by a failure of the user's consumer logic can result in a dropped message.


## Shutting down

```cpp

// Close the producer. The method `waitForConfirms()` will block until all sent messages are confirmed by the broker.
producer->waitForConfirms();

// Cancel the consumer.
consumer->cancelAndDrain();

// Close the connections to the broker. After calling `rmqa::VHost::close()`, the library will not reconnect to the broker anymore.
vhost->close();

```
