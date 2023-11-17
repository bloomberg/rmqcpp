#include <rmqa_connectionstring.h>
#include <rmqa_consumer.h>
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

#include <ball_severityutil.h>
#include <ball_streamobserver.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_optional.h>
#include <bsl_vector.h>
#include <bsls_atomic.h>

#include <chrono>
#include <string>
#include <thread>

using namespace BloombergLP;
using namespace std::chrono_literals;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RESUME.CONSUMER")

bsls::AtomicInt consumedCount;
} // namespace

void configLog(ball::Severity::Level level)
{
    static ball::LoggerManagerConfiguration configuration;
    static bsl::shared_ptr<ball::StreamObserver> loggingObserver =
        bsl::make_shared<ball::StreamObserver>(&bsl::cout);

    configuration.setDefaultThresholdLevelsIfValid(level);

    static ball::LoggerManagerScopedGuard guard(configuration);
    ball::LoggerManager::singleton().registerObserver(loggingObserver,
                                                      "stdout");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "USAGE: " << argv[0] << " <amqp uri>\n";
        return 1;
    }

    configLog(ball::Severity::e_INFO);
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
    rmqt::ExchangeHandle exch = topology.addExchange("exchange");
    rmqt::QueueHandle queue   = topology.addQueue("queue1");

    topology.bind(exch, queue, "routing_key");

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

    bsl::vector<uint8_t> messageVector = {5, 3, 1};

    // `send` will block until a confirm is received if the
    // `maxOutstandingConfirms` limit is reached

    uint8_t i = 0;
    while (i++ < 10) {
        messageVector.push_back(i);
        rmqt::Message message(
            bsl::make_shared<bsl::vector<uint8_t> >(messageVector));
        const rmqp::Producer::SendStatus sendResult = producer->send(
            message,
            "routing_key",
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
                    // and/or raise an alarm for investigation - your message is
                    // not delivered to all (or perhaps any) of the queues it
                    // was intended for.
                    std::cerr << "Message not confirmed: " << message.guid()
                              << " for routing key " << routingKey << " "
                              << response << "\n";
                }
            });

        if (sendResult != rmqp::Producer::SENDING) {
            if (sendResult == rmqp::Producer::DUPLICATE) {
                std::cerr
                    << "Failed to send message: " << message.guid()
                    << " because an identical GUID is already outstanding\n";
            }
            else {
                std::cerr << "Unknown send failure for message: "
                          << message.guid() << "\n";
            }
            return 1;
        }
    }

    rmqt::Result<rmqa::Consumer> consumerResult = vhost->createConsumer(
        topology,
        queue,
        [](rmqp::MessageGuard& messageGuard) {
            std::this_thread::sleep_for(2000ms);
            messageGuard.ack();
            ++consumedCount;
            // std::cout << messageGuard.message() << std::endl;
        },              // Consumer callback invoked on each message
        "consumer-1.0", // Consumer Label (shows in Management UI)
        10              // prefetch count
    );

    if (!consumerResult) {
        return -1;
    }

    bsl::shared_ptr<rmqa::Consumer> consumer = consumerResult.value();

    // Wait for consumer to start consuming
    std::this_thread::sleep_for(10000ms);
    std::cout << "Number of messages consumed before cancelling: "
              << consumedCount << "\n";
    consumer->cancel();

    consumer->resume();
    // Wait for consumer to consume messages after resuming
    std::this_thread::sleep_for(10000ms);

    while (i++ < 40) {
        messageVector.push_back(i);
        rmqt::Message message(
            bsl::make_shared<bsl::vector<uint8_t> >(messageVector));
        const rmqp::Producer::SendStatus sendResult = producer->send(
            message,
            "routing_key",
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
                    // and/or raise an alarm for investigation - your message is
                    // not delivered to all (or perhaps any) of the queues it
                    // was intended for.
                    std::cerr << "Message not confirmed: " << message.guid()
                              << " for routing key " << routingKey << " "
                              << response << "\n";
                }
            });

        if (sendResult != rmqp::Producer::SENDING) {
            if (sendResult == rmqp::Producer::DUPLICATE) {
                std::cerr
                    << "Failed to send message: " << message.guid()
                    << " because an identical GUID is already outstanding\n";
            }
            else {
                std::cerr << "Unknown send failure for message: "
                          << message.guid() << "\n";
            }
            return 1;
        }
    }

    std::cout << "Total number of messages consumed: " << consumedCount;

    // placeholder to keep channel open while resuming
    uint16_t placeholder = 0;
    std::cin >> placeholder;
}
