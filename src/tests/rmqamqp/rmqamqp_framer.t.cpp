// Copyright 2020-2023 Bloomberg Finance L.P.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <rmqamqp_framer.h>

#include <rmqamqpt_connectionmethod.h>
#include <rmqamqpt_connectionopen.h>
#include <rmqamqpt_writer.h>
#include <rmqio_serializedframe.h>
#include <rmqt_fieldvalue.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;

using namespace ::testing;

namespace {

const size_t BYTES_HEADER_WITH_MESSAGEID_DELIVERY_MODE = 52;
const size_t BYTES_HEADER_WITH_MESSAGEID               = 51;

class ContentDecodeTests : public ::testing::Test {
  public:
    static rmqt::Message
    messageMaker(const bsl::string& message,
                 const bsl::shared_ptr<rmqt::FieldTable>& headers =
                     bsl::shared_ptr<rmqt::FieldTable>())
    {
        return rmqt::Message(
            bsl::make_shared<bsl::vector<uint8_t> >(
                bsl::vector<uint8_t>(message.begin(), message.end())),
            "", // generate random messageId
            headers);
    }

    rmqamqpt::Frame
    makeHeaderFrame(rmqamqpt::Constants::AMQPFrameType frameType,
                    uint16_t channel,
                    const rmqamqpt::ContentHeader& contentHeader)
    {

        const size_t encodedPayloadSize = contentHeader.encodedSize();

        bsl::shared_ptr<bsl::vector<uint8_t> > data =
            bsl::make_shared<bsl::vector<uint8_t> >();

        rmqamqpt::Writer writer(data.get());
        rmqamqpt::Types::write(writer, static_cast<uint8_t>(frameType));
        rmqamqpt::Types::write(writer, bdlb::BigEndianUint16::make(channel));
        rmqamqpt::Types::write(writer,
                               bdlb::BigEndianUint32::make(encodedPayloadSize));

        rmqamqpt::ContentHeader::encode(writer, contentHeader);
        rmqamqpt::Types::write(writer, rmqamqpt::Constants::FRAME_END);

        assert(data->size() ==
               rmqamqpt::Frame::calculateFrameSize(encodedPayloadSize));

        return rmqamqpt::Frame(frameType, channel, data);
    }
    rmqamqpt::Frame makeBodyFrame(rmqamqpt::Constants::AMQPFrameType frameType,
                                  uint16_t channel,
                                  const rmqamqpt::ContentBody& contentBody)
    {

        const size_t encodedPayloadSize = contentBody.dataLength();

        bsl::shared_ptr<bsl::vector<uint8_t> > data =
            bsl::make_shared<bsl::vector<uint8_t> >();

        rmqamqpt::Writer writer(data.get());
        rmqamqpt::Types::write(writer, static_cast<uint8_t>(frameType));
        rmqamqpt::Types::write(writer, bdlb::BigEndianUint16::make(channel));
        rmqamqpt::Types::write(writer,
                               bdlb::BigEndianUint32::make(encodedPayloadSize));

        rmqamqpt::ContentBody::encode(writer, contentBody);
        rmqamqpt::Types::write(writer, rmqamqpt::Constants::FRAME_END);

        assert(data->size() ==
               (sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) +
                sizeof(uint32_t) + encodedPayloadSize));

        return rmqamqpt::Frame(frameType, channel, data);
    }
};

class ContentEncodeTests : public ::testing::Test {

  public:
    static const size_t MAX_FRAME_SIZE = 59;
    ContentEncodeTests()
    : framer()
    , frames()
    {
        framer.setMaxFrameSize(MAX_FRAME_SIZE);
    }
    rmqamqp::Framer framer;
    bsl::vector<rmqamqpt::Frame> frames;
};
} // namespace

TEST(Framer, Heartbeat)
{
    rmqamqpt::Frame hb = rmqamqp::Framer::makeHeartbeatFrame();

    rmqamqp::Framer framer;

    uint16_t receiveChannel;
    rmqamqp::Message receiveMessage;
    EXPECT_THAT(framer.appendFrame(&receiveChannel, &receiveMessage, hb),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(receiveMessage.is<rmqamqpt::Heartbeat>());

    EXPECT_THAT(receiveChannel, Eq(0));
}

TEST(Framer, Method)
{
    rmqamqpt::Frame frame;
    const uint16_t channel = 10; // Note this is actually an invalid channel for
                                 // connection.open but this is just a test

    {
        rmqamqpt::ConnectionOpen open;
        rmqamqp::Framer::makeMethodFrame(
            &frame, channel, rmqamqpt::ConnectionMethod(open));
    }

    rmqamqp::Framer framer;

    uint16_t receiveChannel;
    rmqamqp::Message receiveMessage;
    EXPECT_THAT(framer.appendFrame(&receiveChannel, &receiveMessage, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(receiveMessage.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = receiveMessage.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::ConnectionMethod>());
    const rmqamqpt::ConnectionMethod& cm = m.the<rmqamqpt::ConnectionMethod>();

    EXPECT_TRUE(cm.is<rmqamqpt::ConnectionOpen>());

    EXPECT_THAT(receiveChannel, Eq(channel));
}

TEST(Framer, SerialiseMethod)
{
    rmqamqpt::ConnectionOpen open;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 0, rmqamqpt::ConnectionMethod(open));

    EXPECT_THAT(frame.type(), Eq(rmqamqpt::Constants::METHOD));

    // Confirm method header (class-id and method-id are as expected)
    EXPECT_THAT(frame.payloadLength(), Gt(4));

    // test frame.payload()[0..4] are as expected
}

TEST(Framer, MessageHeartbeatSerialize)
{
    const rmqamqp::Message msg((rmqamqpt::Heartbeat()));

    bsl::vector<rmqamqpt::Frame> frames;

    uint16_t irrelevant = 123; // channel is ignored for heartbeat messages

    rmqamqp::Framer framer;
    framer.makeFrames(&frames, irrelevant, msg);

    EXPECT_THAT(frames.size(), Eq(1));

    const rmqio::SerializedFrame serialized(frames[0]);

    EXPECT_THAT(serialized.frameLength(), Eq(8));

    const uint8_t expected_bytes[] = {
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCE};

    EXPECT_THAT(expected_bytes, ElementsAreArray(serialized.serialized(), 8));
}

TEST(Framer, MakeHeartbeatEquivalentToSerialize)
{
    const rmqamqp::Message msg((rmqamqpt::Heartbeat()));

    bsl::vector<rmqamqpt::Frame> frames;

    uint16_t irrelevant = 123; // channel is ignored for heartbeat messages

    rmqamqp::Framer framer;
    framer.makeFrames(&frames, irrelevant, msg);

    EXPECT_THAT(frames.size(), Eq(1));

    EXPECT_THAT(frames[0], Eq(rmqamqp::Framer::makeHeartbeatFrame()));
}

TEST_F(ContentDecodeTests, ContentEncodeDecode)
{
    rmqamqp::Message sendMessage(messageMaker("Hello World"));
    rmqamqp::Message receiveMessage;
    uint16_t inboundChannel, outboundChannel = 2;

    rmqamqp::Framer framer;
    bsl::vector<rmqamqpt::Frame> frames;
    framer.makeFrames(&frames, outboundChannel, sendMessage);

    for (bsl::vector<rmqamqpt::Frame>::const_iterator frame = frames.begin();
         framer.appendFrame(&inboundChannel, &receiveMessage, *frame) ==
         rmqamqp::Framer::PARTIAL;
         ++frame)
        ;
    EXPECT_THAT(receiveMessage.the<rmqt::Message>(),
                Eq(sendMessage.the<rmqt::Message>()));
}

TEST_F(ContentDecodeTests, ContentEncodeDecodeWithMessageHeaders)
{
    bsl::shared_ptr<rmqt::FieldTable> headers(
        bsl::make_shared<rmqt::FieldTable>());
    (*headers)["this_test"] = rmqt::FieldValue(bsl::string("will pass"));
    rmqt::Message msg       = messageMaker("Hello World", headers);
    msg.updateDeliveryMode(rmqt::DeliveryMode::NON_PERSISTENT);
    rmqamqp::Message sendMessage(msg);
    rmqamqp::Message receiveMessage;
    uint16_t inboundChannel, outboundChannel = 2;

    rmqamqp::Framer framer;
    bsl::vector<rmqamqpt::Frame> frames;
    framer.makeFrames(&frames, outboundChannel, sendMessage);

    for (bsl::vector<rmqamqpt::Frame>::const_iterator frame = frames.begin();
         framer.appendFrame(&inboundChannel, &receiveMessage, *frame) ==
         rmqamqp::Framer::PARTIAL;
         ++frame)
        ;
    EXPECT_THAT(receiveMessage.the<rmqt::Message>(),
                Eq(sendMessage.the<rmqt::Message>()));
}

// No need to send extra byte to set delivery mode non-persistent.
// Because broker has non-persistent by default.
TEST_F(ContentDecodeTests, ContentEncodeDecodeWithoutDeliveryMode)
{
    rmqt::Message msg = messageMaker("Hello World");
    msg.updateDeliveryMode(rmqt::DeliveryMode::NON_PERSISTENT);
    rmqamqp::Message sendMessage(msg);
    rmqamqp::Message receiveMessage;
    uint16_t inboundChannel, outboundChannel = 2;

    rmqamqp::Framer framer;
    bsl::vector<rmqamqpt::Frame> frames;
    framer.makeFrames(&frames, outboundChannel, sendMessage);

    for (bsl::vector<rmqamqpt::Frame>::const_iterator frame = frames.begin();
         framer.appendFrame(&inboundChannel, &receiveMessage, *frame) ==
         rmqamqp::Framer::PARTIAL;
         ++frame)
        ;
    EXPECT_THAT(frames[0].payloadLength(),
                Eq(BYTES_HEADER_WITH_MESSAGEID_DELIVERY_MODE)); // header
    EXPECT_THAT(receiveMessage.the<rmqt::Message>(),
                Eq(sendMessage.the<rmqt::Message>()));
}

TEST_F(ContentDecodeTests, ContentEncodeDecodeWithDeliveryMode)
{
    rmqt::Message msg = messageMaker("Hello World");
    rmqamqp::Message sendMessage(msg);
    rmqamqp::Message receiveMessage;
    uint16_t inboundChannel, outboundChannel = 2;

    rmqamqp::Framer framer;
    bsl::vector<rmqamqpt::Frame> frames;
    framer.makeFrames(&frames, outboundChannel, sendMessage);

    for (bsl::vector<rmqamqpt::Frame>::const_iterator frame = frames.begin();
         framer.appendFrame(&inboundChannel, &receiveMessage, *frame) ==
         rmqamqp::Framer::PARTIAL;
         ++frame)
        ;
    EXPECT_THAT(frames[0].payloadLength(),
                Eq(BYTES_HEADER_WITH_MESSAGEID_DELIVERY_MODE)); // header
    EXPECT_THAT(receiveMessage.the<rmqt::Message>(),
                Eq(sendMessage.the<rmqt::Message>()));
}

TEST_F(ContentDecodeTests, ContentHeaderBodyEncodeDecode)
{
    rmqamqpt::Frame frame;
    uint16_t channel;
    rmqamqp::Framer framer;
    rmqamqp::Message received;

    rmqamqpt::ContentHeader contentHeader(
        rmqamqpt::Constants::BASIC, 5, rmqamqpt::BasicProperties());
    frame = makeHeaderFrame(rmqamqpt::Constants::HEADER, 2, contentHeader);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 2);

    const uint8_t* data = reinterpret_cast<const uint8_t*>("hello");
    rmqamqpt::ContentBody contentBody(data, 5);
    frame = makeBodyFrame(rmqamqpt::Constants::BODY, 2, contentBody);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));
    EXPECT_EQ(channel, 2);

    EXPECT_TRUE(received.is<rmqt::Message>());
    rmqt::Message message = received.the<rmqt::Message>();
    EXPECT_EQ(message.payloadSize(), 5);
    EXPECT_EQ(memcmp(message.payload(), data, 5), 0);
}

TEST_F(ContentDecodeTests, ContentBodyWithoutHeader)
{
    rmqamqpt::Frame frame;
    uint16_t channel;
    rmqamqp::Framer framer;
    rmqamqp::Message received;

    rmqamqpt::ContentBody contentBody(reinterpret_cast<const uint8_t*>("hello"),
                                      5);
    frame = makeBodyFrame(rmqamqpt::Constants::BODY, 2, contentBody);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::CHANNEL_EXCEPTION));
    EXPECT_EQ(channel, 2);
}

TEST_F(ContentDecodeTests, MoreContentBodyThanSpecifiedInHeader)
{
    rmqamqpt::Frame frame;
    uint16_t channel;
    rmqamqp::Framer framer;
    rmqamqp::Message received;

    rmqamqpt::ContentHeader contentHeader(
        rmqamqpt::Constants::BASIC, 5, rmqamqpt::BasicProperties());
    frame = makeHeaderFrame(rmqamqpt::Constants::HEADER, 2, contentHeader);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 2);

    rmqamqpt::ContentBody contentBody(
        reinterpret_cast<const uint8_t*>("hello world"), 11);
    frame = makeBodyFrame(rmqamqpt::Constants::BODY, 2, contentBody);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::CHANNEL_EXCEPTION));
    EXPECT_EQ(channel, 2);
}

TEST_F(ContentDecodeTests, MultipleContentBody)
{
    rmqamqpt::Frame frame;
    uint16_t channel;
    rmqamqp::Framer framer;
    rmqamqp::Message received;

    rmqamqpt::ContentHeader contentHeader(
        rmqamqpt::Constants::BASIC, 10, rmqamqpt::BasicProperties());
    frame = makeHeaderFrame(rmqamqpt::Constants::HEADER, 2, contentHeader);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 2);

    const uint8_t* data1 = reinterpret_cast<const uint8_t*>("hello");
    rmqamqpt::ContentBody contentBody1(data1, 5);
    frame = makeBodyFrame(rmqamqpt::Constants::BODY, 2, contentBody1);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 2);

    const uint8_t* data2 = reinterpret_cast<const uint8_t*>("world");
    rmqamqpt::ContentBody contentBody2(data2, 5);
    frame = makeBodyFrame(rmqamqpt::Constants::BODY, 2, contentBody2);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));
    EXPECT_EQ(channel, 2);

    EXPECT_TRUE(received.is<rmqt::Message>());
    rmqt::Message message = received.the<rmqt::Message>();
    EXPECT_EQ(message.payloadSize(), 10);
    EXPECT_EQ(memcmp(message.payload(), data1, 5), 0);
    EXPECT_EQ(memcmp(message.payload() + 5, data2, 5), 0);
}

TEST_F(ContentDecodeTests, ClearChannelTest)
{
    rmqamqpt::Frame frame;
    uint16_t channel;
    rmqamqp::Framer framer;
    rmqamqp::Message received;

    rmqamqpt::ContentHeader contentHeader(
        rmqamqpt::Constants::BASIC, 10, rmqamqpt::BasicProperties());
    frame = makeHeaderFrame(rmqamqpt::Constants::HEADER, 2, contentHeader);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 2);

    framer.clearChannel(channel);

    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 2);
}

TEST_F(ContentEncodeTests, ZeroBodySizeDoesNotProduceBodyFrame)
{
    framer.makeFrames(&frames, 2, rmqamqp::Message(rmqt::Message()));

    EXPECT_THAT(frames, SizeIs(1));
    EXPECT_TRUE(*reinterpret_cast<const bdlb::BigEndianUint64*>(
                    frames[0].rawData() + rmqamqpt::Frame::frameHeaderSize() +
                    4) == 0); // body size field inside content header frame
    EXPECT_THAT(frames[0].payloadLength(),
                Eq(16)); // Content-header size with delivery mode
}

TEST_F(ContentEncodeTests, SmallMessageIsEncodedInOneFrame)
{
    const size_t messageBytes = 20;
    rmqamqp::Message theMessage(
        rmqt::Message(bsl::make_shared<bsl::vector<uint8_t> >(messageBytes)));
    framer.makeFrames(&frames, 2u, theMessage);

    EXPECT_THAT(frames, SizeIs(2));
    EXPECT_TRUE(
        *reinterpret_cast<const bdlb::BigEndianUint64*>(
            frames[0].rawData() + rmqamqpt::Frame::frameHeaderSize() + 4) ==
        messageBytes); // body size field inside content header frame
    EXPECT_THAT(frames[0].payloadLength(),
                Eq(BYTES_HEADER_WITH_MESSAGEID_DELIVERY_MODE)); // header
    EXPECT_THAT(frames[1].payloadLength(), Eq(messageBytes));   // body frame
}

TEST_F(ContentEncodeTests, LargerMessageIsEncodedInTwoFrames)
{
    const size_t messageBytes = 60;
    size_t firstFrame  = MAX_FRAME_SIZE - rmqamqpt::Frame::frameOverhead();
    size_t secondFrame = messageBytes - firstFrame;
    framer.makeFrames(
        &frames,
        2,
        rmqamqp::Message(rmqt::Message(
            bsl::make_shared<bsl::vector<uint8_t> >(messageBytes))));
    EXPECT_THAT(frames, SizeIs(3));
    EXPECT_TRUE(
        *reinterpret_cast<const bdlb::BigEndianUint64*>(
            frames[0].rawData() + rmqamqpt::Frame::frameHeaderSize() + 4) ==
        messageBytes); // body size field inside content header frame
    EXPECT_THAT(frames[0].payloadLength(),
                Eq(BYTES_HEADER_WITH_MESSAGEID_DELIVERY_MODE)); // header
    EXPECT_THAT(frames[1].payloadLength(), Eq(firstFrame));     // body frame 1
    EXPECT_THAT(frames[2].payloadLength(), Eq(secondFrame));    // body frame 2
}

TEST_F(ContentEncodeTests, EncodeContentHeaderBodyFrames)
{
    const size_t payLoadSize = 40;
    rmqamqp::Message received;
    uint16_t channel;
    rmqt::Message msg(bsl::make_shared<bsl::vector<uint8_t> >(payLoadSize));

    rmqamqpt::Frame headerFrame =
        rmqamqp::Framer::makeContentHeaderFrame(msg, 5);
    EXPECT_THAT(framer.appendFrame(&channel, &received, headerFrame),
                Eq(rmqamqp::Framer::PARTIAL));
    EXPECT_EQ(channel, 5);

    const size_t encodedBodyFrameSize =
        rmqamqpt::Frame::calculateFrameSize(payLoadSize);
    channel                   = 0;
    rmqamqpt::Frame bodyFrame = rmqamqp::Framer::makeContentBodyFrame(
        msg.payload(), encodedBodyFrameSize, payLoadSize, 5);
    EXPECT_THAT(framer.appendFrame(&channel, &received, bodyFrame),
                Eq(rmqamqp::Framer::OK));
    EXPECT_EQ(channel, 5);
}
