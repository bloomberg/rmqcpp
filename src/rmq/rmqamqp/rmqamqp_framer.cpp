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

#include <rmqamqpt_constants.h>
#include <rmqamqpt_contentheader.h>
#include <rmqamqpt_heartbeat.h>
#include <rmqamqpt_method.h>
#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>

#include <bsl_algorithm.h>
#include <bsl_cstdint.h>
#include <bsl_istream.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.FRAMER")

class MessageSerializer {
  public:
    MessageSerializer(bsl::vector<rmqamqpt::Frame>* frames,
                      uint16_t channel,
                      size_t maxFrameSize)
    : d_frames(frames)
    , d_channel(channel)
    , d_maxFrameSize(maxFrameSize)
    {
    }

    void operator()(const rmqt::Message& message) const
    {
        d_frames->push_back(
            rmqamqp::Framer::makeContentHeaderFrame(message, d_channel));

        const size_t frameSize =
            d_maxFrameSize - rmqamqpt::Frame::frameOverhead();
        for (bsl::size_t i = 0; i < message.payloadSize(); i += frameSize) {
            const size_t encodedPayloadSize =
                bsl::min(frameSize, message.payloadSize() - i);
            const size_t encodedFrameSize =
                rmqamqpt::Frame::calculateFrameSize(encodedPayloadSize);

            d_frames->push_back(
                rmqamqp::Framer::makeContentBodyFrame(message.payload() + i,
                                                      encodedFrameSize,
                                                      encodedPayloadSize,
                                                      d_channel));
        }
    }
    void operator()(const rmqamqpt::Heartbeat&) const
    {
        d_frames->push_back(Framer::makeHeartbeatFrame());
    }

    void operator()(const rmqamqpt::Method& method) const
    {
        d_frames->push_back(rmqamqpt::Frame());
        Framer::makeMethodFrame(&d_frames->back(), d_channel, method);
    }

    void operator()(const bslmf::Nil&) const {}

  private:
    bsl::vector<rmqamqpt::Frame>* d_frames;
    const uint16_t d_channel;
    const bsl::size_t d_maxFrameSize;
};
} // namespace

Framer::Framer()
: d_channelContentMakers()
, d_maxFrameSize(rmqamqpt::Frame::getMaxFrameSize())
{
}

void Framer::setMaxFrameSize(bsl::size_t maxSize) { d_maxFrameSize = maxSize; }

Framer::ReturnCode Framer::appendFrame(uint16_t* receiveChannel,
                                       rmqamqp::Message* receiveMessage,
                                       const rmqamqpt::Frame& frame)
{
    *receiveChannel = frame.channel();
    switch (frame.type()) {
        case rmqamqpt::Constants::METHOD: {
            if (d_channelContentMakers.find(frame.channel()) !=
                d_channelContentMakers.end()) {
                BALL_LOG_ERROR << "Channel exception: Unable to decode frames "
                                  "in buffer for channel ["
                               << frame.channel() << "].";
                return CHANNEL_EXCEPTION;
            }

            receiveMessage->createInPlace<rmqamqpt::Method>();
            if (!rmqamqpt::Method::Util::decode(
                    &receiveMessage->the<rmqamqpt::Method>(),
                    frame.rawData() + rmqamqpt::Frame::frameHeaderSize(),
                    frame.payloadLength())) {
                BALL_LOG_ERROR << "Failed to decode Method for channel: "
                               << frame.channel()
                               << ". payloadLength: " << frame.payloadLength();
                return CHANNEL_EXCEPTION;
            }

            return OK;
        } break;
        case rmqamqpt::Constants::HEARTBEAT: {
            receiveMessage->createInPlace<rmqamqpt::Heartbeat>();
            return OK;
        }
        case rmqamqpt::Constants::HEADER: {
            if (d_channelContentMakers.find(frame.channel()) !=
                d_channelContentMakers.end()) {
                BALL_LOG_ERROR << " Channel exception: Unable to decode frames "
                                  "in buffer for channel ["
                               << frame.channel() << "].";
                return CHANNEL_EXCEPTION;
            }

            rmqamqpt::ContentHeader contentHeader;
            if (!rmqamqpt::ContentHeader::decode(
                    &contentHeader, frame.payload(), frame.payloadLength())) {
                BALL_LOG_ERROR
                    << "Failed to decode content header for channel: "
                    << frame.channel()
                    << ". payloadLength: " << frame.payloadLength();
                return CHANNEL_EXCEPTION;
            }

            bsl::shared_ptr<ContentMaker> maker =
                bsl::make_shared<ContentMaker>(contentHeader);

            if (maker->done()) { // It's possible a message has just the header
                                 // and no body
                receiveMessage->assignTo<rmqt::Message>(maker->message());
                return OK;
            }

            d_channelContentMakers[frame.channel()] = maker;
            return PARTIAL;
        }
        case rmqamqpt::Constants::BODY: {
            ChannelContentMaker::iterator it =
                d_channelContentMakers.find(frame.channel());
            if (it == d_channelContentMakers.end()) {
                BALL_LOG_ERROR
                    << "Channel Exception: Received content body frame without "
                       "prior header frame on channel [ "
                    << frame.channel() << "].";
                return CHANNEL_EXCEPTION;
            }

            rmqamqpt::ContentBody contentBody;
            rmqamqpt::ContentBody::decode(
                &contentBody, frame.payload(), frame.payloadLength());

            ContentMaker::ReturnCode rc =
                it->second->appendContentBody(contentBody);
            if (rc == ContentMaker::ERROR) {
                BALL_LOG_ERROR << "Channel exception: size of content body is "
                                  "more than specified in the content header ["
                               << frame.channel() << "].";
                return CHANNEL_EXCEPTION;
            }

            if (rc == ContentMaker::DONE) {
                receiveMessage->assignTo<rmqt::Message>(it->second->message());
                d_channelContentMakers.erase(it);
                return OK;
            }
            return PARTIAL;
        }
        default: {
            BALL_LOG_ERROR << "Unhandled frame type: " << frame.type();
        }
    }

    return CONNECTION_EXCEPTION;
}

void Framer::clearChannel(uint16_t channel)
{
    d_channelContentMakers.erase(channel);
}

void Framer::makeMethodFrame(rmqamqpt::Frame* frame,
                             uint16_t channel,
                             const rmqamqpt::Method& method)
{

    const size_t encodedPayloadSize = method.encodedSize();
    const size_t encodedFrameSize =
        rmqamqpt::Frame::calculateFrameSize(encodedPayloadSize);

    bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >();
    data->reserve(encodedFrameSize);
    rmqamqpt::Writer writer(data.get());

    encodeFrameHeader(
        writer, rmqamqpt::Constants::METHOD, channel, encodedPayloadSize);
    rmqamqpt::Method::Util::encode(writer, method);
    encodeFrameEnd(writer);

    *frame = rmqamqpt::Frame(rmqamqpt::Constants::METHOD, channel, data);
}

rmqamqpt::Frame Framer::makeContentBodyFrame(const uint8_t* message,
                                             const size_t encodedFrameSize,
                                             const size_t encodedPayloadSize,
                                             uint16_t channel)
{

    bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >();
    data->reserve(encodedFrameSize);
    rmqamqpt::Writer writer(data.get());

    rmqamqp::Framer::encodeFrameHeader(
        writer, rmqamqpt::Constants::BODY, channel, encodedPayloadSize);
    writer.write(message, encodedPayloadSize);
    rmqamqp::Framer::encodeFrameEnd(writer);

    return rmqamqpt::Frame(rmqamqpt::Constants::BODY, channel, data);
}

rmqamqpt::Frame Framer::makeContentHeaderFrame(const rmqt::Message& message,
                                               uint16_t channel)
{

    const rmqamqpt::ContentHeader header(rmqamqpt::Constants::BASIC, message);

    const size_t encodedPayloadSize = header.encodedSize();
    const size_t encodedFrameSize =
        rmqamqpt::Frame::calculateFrameSize(encodedPayloadSize);

    const bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >();
    data->reserve(encodedFrameSize);
    rmqamqpt::Writer writer(data.get());

    rmqamqp::Framer::encodeFrameHeader(
        writer, rmqamqpt::Constants::HEADER, channel, encodedPayloadSize);
    rmqamqpt::ContentHeader::encode(writer, header);
    rmqamqp::Framer::encodeFrameEnd(writer);

    return rmqamqpt::Frame(rmqamqpt::Constants::HEADER, channel, data);
}

rmqamqpt::Frame Framer::makeHeartbeatFrame()
{

    const bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >();
    data->reserve(rmqamqpt::Frame::frameOverhead());
    rmqamqpt::Writer writer(data.get());

    rmqamqp::Framer::encodeFrameHeader(
        writer, rmqamqpt::Constants::HEARTBEAT, 0, 0);
    rmqamqp::Framer::encodeFrameEnd(writer);

    return rmqamqpt::Frame(rmqamqpt::Constants::HEARTBEAT, 0, data);
}

void Framer::makeFrames(bsl::vector<rmqamqpt::Frame>* frames,
                        uint16_t channel,
                        const rmqamqp::Message& message) const
{
    MessageSerializer serializer(frames, channel, d_maxFrameSize);

    message.apply(serializer);
}

void Framer::encodeFrameHeader(rmqamqpt::Writer& output,
                               uint8_t type,
                               uint16_t channel,
                               size_t payloadLength)
{
    rmqamqpt::Types::write(output, type);
    rmqamqpt::Types::write(output, bdlb::BigEndianUint16::make(channel));
    rmqamqpt::Types::write(output, bdlb::BigEndianUint32::make(payloadLength));
}

void Framer::encodeFrameEnd(rmqamqpt::Writer& output)
{
    rmqamqpt::Types::write(output, rmqamqpt::Constants::FRAME_END);
}

} // namespace rmqamqp
} // namespace BloombergLP
