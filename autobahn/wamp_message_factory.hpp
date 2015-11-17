#ifndef WAMP_MESSAGE_FACTORY_HPP
#define WAMP_MESSAGE_FACTORY_HPP
///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2014 Tavendo GmbH
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include "wamp_message_type.hpp"
#include "autobahn/exceptions.hpp"
#include <stdexcept>

namespace autobahn {

inline wamp_error make_wamp_error(wamp_message& message){
    // [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri]
    // [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri, Arguments|list]
    // [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri, Arguments|list, ArgumentsKw|dict]
    std::string uri;
    msgpack::object args = EMPTY_ARGUMENTS;
    msgpack::object details = EMPTY_DETAILS;
    msgpack::object kw_args = EMPTY_KW_ARGUMENTS;

    // message length
    //
    if (message.size() != 5 && message.size() != 6 && message.size() != 7) {
        throw protocol_error("invalid ERROR message structure - length must be 5, 6 or 7");
    }

    // REQUEST.Type|int
    //
    if (message[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid ERROR message structure - REQUEST.Type must be an integer");
    }
    message_type request_type = static_cast<message_type>(message[1].as<int>());

    // REQUEST.Request|id
    if (message[2].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid ERROR message structure - REQUEST.Request must be an integer");
    }
    uint64_t request_id = message[2].as<uint64_t>();

    // Details
    if (message[3].type != msgpack::type::MAP) {
        throw protocol_error("invalid ERROR message structure - Details must be a dictionary");
    }
    details = message[3];

    // Error|uri
    if (message[4].type != msgpack::type::STR) {
        throw protocol_error("invalid ERROR message - Error must be a string (URI)");
    }
    uri = message[4].as<std::string>();

    // Arguments|list
    if (message.size() > 5) {
        if (message[5].type != msgpack::type::ARRAY) {
            throw protocol_error("invalid ERROR message structure - Arguments must be a list");
        }
        args = message[5];
    }

    // ArgumentsKw|list
    if (message.size() > 6) {
        if (message[6].type != msgpack::type::MAP) {
            throw protocol_error("invalid ERROR message structure - ArgumentsKw must be a dictionary");
        }
        kw_args = message[6];
    }
    return wamp_error(request_type, request_id, uri, details, args, kw_args, message.zone());
}

inline wamp_call_result make_wamp_call_result(wamp_message &message){
    // [RESULT, CALL.Request|id, Details|dict]
    // [RESULT, CALL.Request|id, Details|dict, YIELD.Arguments|list]
    // [RESULT, CALL.Request|id, Details|dict, YIELD.Arguments|list, YIELD.ArgumentsKw|dict]
    auto result = wamp_call_result(message.zone());

    if (message.size() != 3 && message.size() != 4 && message.size() != 5) {
        throw protocol_error("RESULT - length must be 3, 4 or 5");
    }

    if (message[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("RESULT - CALL.Request must be an id");
    }

    result.set_id(message[1].as<uint64_t>());

    if (message[2].type != msgpack::type::MAP) {
        throw protocol_error("RESULT - Details must be a dictionary");
    }

    if (message.size() > 3) {
        if (message[3].type != msgpack::type::ARRAY) {
            throw protocol_error("RESULT - YIELD.Arguments must be a list");
        }
        result.set_arguments(message[3]);

        if (message.size() > 4) {
            if (message[4].type != msgpack::type::MAP) {
                throw protocol_error("RESULT - YIELD.ArgumentsKw must be a dictionary");
            }
            result.set_kw_arguments(message[4]);
        }
    }
    return result;
}

}
#endif // WAMP_MESSAGE_FACTORY_HPP
