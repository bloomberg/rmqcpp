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

#ifndef INCLUDED_RMQAMQP_MESSAGESTORE
#define INCLUDED_RMQAMQP_MESSAGESTORE

#include <rmqt_message.h>

#include <ball_log.h>
#include <bdlb_guid.h>
#include <bdlb_guidutil.h>
#include <bdlt_currenttime.h>
#include <bslmt_once.h>
#include <bsls_atomic.h>
#include <bsls_timeinterval.h>

#include <bsl_algorithm.h>
#include <bsl_cstddef.h>
#include <bsl_map.h>
#include <bsl_memory.h>
#include <bsl_unordered_map.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

//@PURPOSE: Store for unacknowledged messages
//
//@CLASSES: rmqamqp::MessageStore

namespace BloombergLP {
namespace rmqamqp {

template <typename Msg>
class MessageStore {
  public:
    typedef bsl::pair<uint64_t, bsl::pair<Msg, bdlt::Datetime> > Entry;
    typedef bsl::vector<Entry> MessageList;

    MessageStore();

    /// Save the message with delivery-tag in the store
    /// Return true if the message was inserted in the store, otherwise log the
    /// error and return false
    bool insert(uint64_t deliveryTag, const Msg& message);

    /// Search message by delivery-tag in the store
    /// Return true and set the message argument if successful search
    bool lookup(uint64_t deliveryTag, Msg* message) const;

    /// Search message by guid in the store
    /// Return true and set the message and deliveryTag arguments, if
    /// successful search
    bool
    lookup(const bdlb::Guid& guid, Msg* message, uint64_t* deliveryTag) const;

    /// Remove message by delivery-tag from the store
    /// Return true and set the message argument if delivery-tag exists
    bool remove(uint64_t deliveryTag, Msg* message, bdlt::Datetime* insertTime);

    /// Remove all the messages till specified delivery-tag from the store and
    /// return those messages
    MessageList removeUntil(uint64_t deliveryTag);

    void swap(MessageStore& messageStore);

    /// Return total messages inside the store
    bsl::size_t count() const { return d_deliveryTagToMsg.size(); };

    /// Return maximum assigned delivery-tag to any message till now
    uint64_t latestTagTilNow() const { return d_latestTagTilNow; }

    uint64_t latestTagInStore() const;
    uint64_t oldestTagInStore() const;

    /// @brief lifetime ID starts at 0 for a MessageStore and increments
    ///        each time the MessageStore is `swap()`'ed out, which must
    ///        happen each time the Channel is closed
    /// @return the current lifetime ID
    bsl::size_t lifetimeId() const { return d_lifetimeId; }

    /// Return messages older than `time` (absolute time)
    MessageList getMessagesOlderThan(const bdlt::Datetime& time) const;

  private:
    MessageStore(const MessageStore&) BSLS_KEYWORD_DELETED;
    MessageStore& operator=(const MessageStore&) BSLS_KEYWORD_DELETED;

    typedef bsl::map<uint64_t, bsl::pair<Msg, bdlt::Datetime> >
        DeliveryTagToMessageMap;
    typedef bsl::unordered_map<bdlb::Guid,
                               typename DeliveryTagToMessageMap::iterator>
        GuidToMessageMap;

    DeliveryTagToMessageMap d_deliveryTagToMsg;
    GuidToMessageMap d_guidToMsg;
    uint64_t d_latestTagTilNow;
    size_t d_lifetimeId;

    BALL_LOG_SET_CLASS_CATEGORY("RMQAMQP.MESSAGESTORE");
}; // class MessageStore

template <typename Msg>
MessageStore<Msg>::MessageStore()
: d_deliveryTagToMsg()
, d_guidToMsg()
, d_latestTagTilNow(0)
, d_lifetimeId(0)
{
}

template <typename Msg>
bool MessageStore<Msg>::insert(uint64_t deliveryTag, const Msg& message)
{
    bsl::pair<typename DeliveryTagToMessageMap::iterator, bool> ret =
        d_deliveryTagToMsg.emplace(
            deliveryTag, bsl::make_pair(message, bdlt::CurrentTime::utc()));
    if (!ret.second) {
        if (message.guid() != (ret.first)->second.first.guid()) {
            BALL_LOG_FATAL << "Different message already present in the store "
                              "with same delivery-tag"
                           << "\nMessage inside the store: "
                           << (ret.first)->second.first
                           << "\nMessage passed as an argument: " << message;
            return false;
        }
        BALL_LOG_ERROR << "Same message already present in the store";
        return false;
    }

    bsl::pair<typename GuidToMessageMap::iterator, bool> guidRc =
        d_guidToMsg.insert(bsl::make_pair(message.guid(), ret.first));

    if (!guidRc.second) {
        BALL_LOG_ERROR << "Duplicate message id (" << message.guid()
                       << ") received from broker with "
                          "different deliveryTags [r"
                       << deliveryTag << ":s" << guidRc.first->second->first
                       << "]";
    }

    d_latestTagTilNow = bsl::max(d_latestTagTilNow, deliveryTag);

    return true;
}

template <typename Msg>
bool MessageStore<Msg>::lookup(uint64_t deliveryTag, Msg* message) const
{
    const typename DeliveryTagToMessageMap::const_iterator it =
        d_deliveryTagToMsg.find(deliveryTag);
    if (it == d_deliveryTagToMsg.end()) {
        return false;
    }
    *message = it->second.first;
    return true;
}

template <typename Msg>
bool MessageStore<Msg>::lookup(const bdlb::Guid& guid,
                               Msg* message,
                               uint64_t* deliveryTag) const
{
    const typename GuidToMessageMap::const_iterator it = d_guidToMsg.find(guid);
    if (it == d_guidToMsg.end()) {
        return false;
    }
    *message     = it->second->second.first;
    *deliveryTag = it->second->first;
    return true;
}

template <typename Msg>
bool MessageStore<Msg>::remove(uint64_t deliveryTag,
                               Msg* message,
                               bdlt::Datetime* insertTime)
{
    const typename DeliveryTagToMessageMap::const_iterator it =
        d_deliveryTagToMsg.find(deliveryTag);
    if (it == d_deliveryTagToMsg.end()) {
        return false;
    }
    *message    = it->second.first;
    *insertTime = it->second.second;
    d_deliveryTagToMsg.erase(it);
    d_guidToMsg.erase(message->guid());
    return true;
}

template <typename Msg>
typename MessageStore<Msg>::MessageList
MessageStore<Msg>::removeUntil(uint64_t deliveryTag)
{
    MessageList removedMessages;
    if (count() == 0) {
        BALL_LOG_INFO << "The message store is empty.";
        return removedMessages;
    }

    if (d_deliveryTagToMsg.find(deliveryTag) == d_deliveryTagToMsg.end()) {
        BALL_LOG_ERROR << deliveryTag << " is not present in the store.";
        return removedMessages;
    }

    typename DeliveryTagToMessageMap::const_iterator it =
        d_deliveryTagToMsg.cbegin();
    for (; (it != d_deliveryTagToMsg.cend()) && (it->first <= deliveryTag);
         ++it) {
        d_guidToMsg.erase(it->second.first.guid());
        removedMessages.push_back(*it);
    }
    d_deliveryTagToMsg.erase(d_deliveryTagToMsg.cbegin(), it);

    return removedMessages;
}

template <typename Msg>
void MessageStore<Msg>::swap(MessageStore& msgStore)
{
    bsl::swap(d_deliveryTagToMsg, msgStore.d_deliveryTagToMsg);
    bsl::swap(d_guidToMsg, msgStore.d_guidToMsg);
    bsl::swap(d_latestTagTilNow, msgStore.d_latestTagTilNow);

    d_lifetimeId++;
}

template <typename Msg>
uint64_t MessageStore<Msg>::latestTagInStore() const
{
    return d_deliveryTagToMsg.empty() ? 0
                                      : (d_deliveryTagToMsg.crbegin())->first;
}

template <typename Msg>
uint64_t MessageStore<Msg>::oldestTagInStore() const
{
    return d_deliveryTagToMsg.empty() ? 0
                                      : (d_deliveryTagToMsg.cbegin())->first;
}

template <typename Msg>
typename MessageStore<Msg>::MessageList
MessageStore<Msg>::getMessagesOlderThan(const bdlt::Datetime& time) const
{
    MessageList results;
    for (typename DeliveryTagToMessageMap::const_iterator it =
             d_deliveryTagToMsg.cbegin();
         it != d_deliveryTagToMsg.cend();
         ++it) {
        if (it->second.second <= time) {
            results.push_back(*it);
        }
        else {
            break;
        }
    }

    return results;
}

} // namespace rmqamqp
} // namespace BloombergLP

#endif
