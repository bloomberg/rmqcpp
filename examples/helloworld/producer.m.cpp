#include <rmqa_connectionstring.h>
#include <rmqa_producer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_topology.h>
#include <rmqa_vhost.h>
#include <rmqp_producer.h>
#include <rmqt_confirmresponse.h>
#include <rmqt_exchange.h>
#include <rmqt_message.h>
#include <rmqt_result.h>
#include <rmqt_vhostinfo.h>

#include <bsl_memory.h>
#include <bsl_optional.h>
#include <bsl_vector.h>

#include <string>

using namespace BloombergLP;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "USAGE: " << argv[0] << " <amqp uri>\n";
        return 1;
    }
    rmqa::RabbitContext rabbit;

    bsl::optional<rmqt::VHostInfo> vhostInfo =
        rmqa::ConnectionString::parse(argv[1]);

    if (!vhostInfo) {
        std::cerr << "Failed to parse connection string: " << argv[1] << "\n";
        return 1;
    }

    // Returns immediately, setup performed on a different thread
    bsl::shared_ptr<rmqa::VHost> vhost = rabbit.createVHostConnection(
        "Sample code for a producer", // Connecion Name Visible in management UI
        vhostInfo.value());

    // How many messages can be awaiting confirmation before `send` blocks
    const uint16_t maxOutstandingConfirms = 10;

    rmqa::Topology topology;
    rmqt::ExchangeHandle exch = topology.addExchange("exch-name");
    rmqt::QueueHandle queue   = topology.addQueue("queue-name");

    topology.bind(exch, queue, "routingkey");

    rmqt::Result<rmqa::Producer> producerResult =
        vhost->createProducer(topology, exch, maxOutstandingConfirms);
    if (!producerResult) {
        // A fatal error such as `exch` not being present in `topology`
        // A disconnection, or  will never permanently fail an operation
        std::cerr << "Error creating connection: " << producerResult.error()
                  << "\n";
        return 1;
    }

    bsl::shared_ptr<rmqa::Producer> producer = producerResult.value();

    std::string json = "[5, 3, 1]";
    rmqt::Message message(
        bsl::make_shared<bsl::vector<uint8_t> >(json.cbegin(), json.cend()));

    // `send` will block until a confirm is received if the
    // `maxOutstandingConfirms` limit is reached
    const rmqp::Producer::SendStatus sendResult = producer->send(
        message,
        "routingkey",
        [](const rmqt::Message& message,
           const bsl::string& routingKey,
           const rmqt::ConfirmResponse& response) {
            // https://www.rabbitmq.com/confirms.html#when-publishes-are-confirmed
            if (response.status() == rmqt::ConfirmResponse::ACK) {
                // Message is now guaranteed to be safe with the broker.
                // Now is the time to reply to the request, commit the
                // database transaction, or ack the RabbitMQ message which
                // triggered this publish
            }
            else {
                // Send error response, rollback transaction, nack message
                // and/or raise an alarm for investigation - your message is not
                // delivered to all (or perhaps any) of the queues it was
                // intended for.
                std::cerr << "Message not confirmed: " << message.guid()
                          << " for routing key " << routingKey << " "
                          << response << "\n";
            }
        });

    if (sendResult != rmqp::Producer::SENDING) {
        if (sendResult == rmqp::Producer::DUPLICATE) {
            std::cerr << "Failed to send message: " << message.guid()
                      << " because an identical GUID is already outstanding\n";
        }
        else {
            std::cerr << "Unknown send failure for message: " << message.guid()
                      << "\n";
        }
        return 1;
    }

    // At this point, `rmqcpp` will attempt to send `message` to the broker. If
    // a disconnection occurs, the message will be retried until a confirmation
    // is received, or the producer is destructed. Applications should consider
    // their message undelivered until `receiveConfirmation` is called.

    // Wait for the confirmation to come back before exiting
    if (!producer->waitForConfirms(/* timeout */)) {
        // Timeout expired
    }
}
