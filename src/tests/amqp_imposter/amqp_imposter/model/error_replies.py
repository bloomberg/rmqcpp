# Copyright 2026 Bloomberg Finance L.P.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from . import method_fields as mf
from . import methods as m

type ErrorReply = m.ChannelClose | m.ConnectionClose


def error_reply(
    reply_code: mf.ReplyCode, reply_text: str, causing_method: m.Method
) -> ErrorReply:
    if reply_code in mf.SOFT_ERRORS:
        return m.ChannelClose(
            reply_code=reply_code,
            reply_text=reply_text,
            class_id=causing_method.CLASS_ID,
            method_id=causing_method.METHOD_ID,
        )
    else:
        return m.ConnectionClose(
            reply_code=reply_code,
            reply_text=reply_text,
            class_id=causing_method.CLASS_ID,
            method_id=causing_method.METHOD_ID,
        )


def optional_reply_codes(method: m.Method | None) -> tuple[mf.ReplyCode, ...]:
    # reply codes that a received method can enable
    # e.g. non-passive QueueDeclare can optionally be answered with PRECONDITION_FAILED
    match method:
        case m.ConnectionOpen():
            return (mf.ReplyCode.INVALID_PATH, mf.ReplyCode.NOT_ALLOWED)
        case m.ChannelOpen():
            return (mf.ReplyCode.RESOURCE_ERROR,)
        case m.ExchangeDeclare(passive=True):
            return (mf.ReplyCode.NOT_FOUND, mf.ReplyCode.ACCESS_REFUSED)
        case m.ExchangeDeclare():
            return (
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.PRECONDITION_FAILED,
                mf.ReplyCode.NOT_ALLOWED,
                mf.ReplyCode.RESOURCE_ERROR,
            )
        case m.ExchangeBind():
            return (
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_ERROR,
            )
        case m.QueueDeclare(passive=True):
            return (
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_LOCKED,
            )
        case m.QueueDeclare():
            return (
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_LOCKED,
                mf.ReplyCode.PRECONDITION_FAILED,
                mf.ReplyCode.RESOURCE_ERROR,
            )
        case m.QueueBind() | m.BasicConsume():
            return (
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_LOCKED,
                mf.ReplyCode.RESOURCE_ERROR,
            )
        case m.BasicGet():
            return (
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_LOCKED,
            )
        case m.BasicPublish(exchange=""):
            return (mf.ReplyCode.ACCESS_REFUSED, mf.ReplyCode.CONTENT_TOO_LARGE)
        case m.BasicPublish():
            return (
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.CONTENT_TOO_LARGE,
            )
        case _:
            return ()


def optional_error_replies(method: m.Method | None) -> tuple[ErrorReply, ...]:
    return tuple(
        error_reply(reply_code, mf.NOT_FORCED_REPLY_TEXT, method)
        for reply_code in optional_reply_codes(method)
        if method is not None
    )
