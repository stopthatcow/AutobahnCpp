#ifndef WAMP_ERROR_HPP
#define WAMP_ERROR_HPP
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

#include "wamp_message.hpp"
#include "autobahn/exceptions.hpp"
#include <stdexcept>

namespace autobahn {
class wamp_error_impl{
public:
    wamp_error_impl(message_type type,
                    uint64_t Id,
                    const std::string &uri,
                    const msgpack::object &details,
                    const msgpack::object &args,
                    const msgpack::object &kwargs,
                    msgpack::unique_ptr<msgpack::zone> &zone)
        : m_type(type)
        , m_id(Id)
        , m_details(details)
        , m_args(args)
        , m_kw_args(kwargs)
    {
        m_zone = std::move(zone);
    }
    message_type m_type;
    uint64_t m_id;
    msgpack::object m_details;
    msgpack::object m_args;
    msgpack::object m_kw_args;
    msgpack::unique_ptr<msgpack::zone> m_zone;
};

class wamp_error : public std::runtime_error {
  public:
    wamp_error(message_type type,
               uint64_t id,
               const std::string &uri,
               const msgpack::object &details,
               const msgpack::object &args,
               const msgpack::object &kwargs,
               msgpack::unique_ptr<msgpack::zone> &zone)
             : std::runtime_error(uri)
    {
        m_impl = std::make_shared<wamp_error_impl>(type, id, uri, details, args, kwargs, zone);
    }

    message_type type(){
        return m_impl->m_type;
    }

    uint64_t id() const{
        return m_impl->m_id;
    }

    const char *uri() const{
        return what();
    }

    template<typename T>
    T args() const{
        return m_impl->m_args.as<T>();
    }

    template<typename T>
    T kw_args() const{
        return m_impl->m_kw_args.as<T>();
    }

    template<typename T>
    T details() const{
        return m_impl->m_details.as<T>();
    }
private:
    std::shared_ptr<wamp_error_impl> m_impl;
};
}

#endif // WAMP_ERROR_HPP
