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

// rmqt_result.h
#ifndef INCLUDED_RMQT_RESULT
#define INCLUDED_RMQT_RESULT

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

typedef enum { TIMEOUT = 408 } ReturnCodes;

/// \brief A result of an operation
///
/// This class provides access to an operation. The result object either
/// contains the value of the result (if the operation was successful) or an
/// error code and message explaining why the operation failed.

template <typename T = void>
class Result {
  private:
    /// Used for 'Safe Bool Idiom'
    /// See https://www.artima.com/cppsource/safebool2.html
    typedef void (Result::*BoolType)() const;
    void thisTypeDoesNotSupportComparisons() const {}

  public:
    // CREATORS
    /// \brief Constructs an error result
    /// \param error Error message
    /// \param resultCode Result code indicating the type of failure
    explicit Result(const bsl::string& error, int resultCode = -1)
    : d_value()
    , d_error(error)
    , d_resultCode(resultCode)
    {
    }

    /// Constructs a result
    /// \param value Result value
    /// \param error Error message (empty indicates success)
    /// \param resultCode Result code (0 indicates success)
    explicit Result(const bsl::shared_ptr<T>& value,
                    const bsl::string& error = bsl::string(),
                    int resultCode           = 0)
    : d_value(value)
    , d_error(error)
    , d_resultCode(resultCode)
    {
    }

    /// Result value
    /// \return Shared pointer to the result value
    const bsl::shared_ptr<T>& value() const { return d_value; }

    /// Error message
    /// \return Error message explaining the future (empty indicates success)
    const bsl::string& error() const { return d_error; }

    /// Return code
    /// \return Return code describing the result (0 indicates success)
    int returnCode() const { return d_resultCode; }

    /// Used to safely convert a `Result` object to
    /// bool.
    operator BoolType() const
    {
        return d_resultCode == 0 ? &Result::thisTypeDoesNotSupportComparisons
                                 : 0;
    }

    bsl::ostream& operator<<(bsl::ostream& os)
    {
        os << "VALUE: " << (d_value ? d_value : "null") << "\n";
        if (!*this) {
            os << "ERROR: " << error();
        }
        return os;
    }

  private:
    bsl::shared_ptr<T> d_value;
    bsl::string d_error;

    /// If resultCode==0, the result is successful.
    int d_resultCode;

}; // class Result

template <>
class Result<void> {
  private:
    typedef void (Result::*BoolType)() const;
    void thisTypeDoesNotSupportComparisons() const {}

  public:
    Result()
    : d_error()
    , d_resultCode(0)
    {
    }

    explicit Result(const bsl::string& error, int resultCode = -1)
    : d_error(error)
    , d_resultCode(resultCode)
    {
    }

    const bsl::string& error() const { return d_error; }
    int returnCode() const { return d_resultCode; }

    operator BoolType() const
    {
        return d_resultCode == 0 ? &Result::thisTypeDoesNotSupportComparisons
                                 : 0;
    }

    bsl::ostream& operator<<(bsl::ostream& os) const
    {
        if (!*this) {
            os << "ERROR: " << error();
        }
        return os;
    }

  private:
    bsl::string d_error;
    /// If resultCode==0, the result is successful.
    int d_resultCode;
};

template <typename T, typename U>
bool operator!=(const Result<T>& lhs, const U&)
{
    lhs.thisTypeDoesNotSupportComparisons();
    return false;
}

template <typename T, typename U>
bool operator==(const Result<T>& lhs, const U&)
{
    lhs.typeTypeDoesNotSupportComparisons();
    return false;
}

typedef bsl::function<void(const bsl::string& errorText, int errorCode)>
    ErrorCallback;
/// ErrorCallback function will be called on client thread,
/// whenever channel or connection will be closed by server.
/// The normal execution of program will not be stopped.
/// The callback will only bubble up the error details to the client.

typedef bsl::function<void()> SuccessCallback;
/// SuccessCallback function will be called on client thread,
/// whenever channel or connection will be restored.

} // namespace rmqt
} // namespace BloombergLP

#endif // ! INCLUDED_RMQT_RESULT
