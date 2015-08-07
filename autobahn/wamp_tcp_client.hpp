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

#ifndef AUTOBAHN_TCP_CLIENT_HPP
#define AUTOBAHN_TCP_CLIENT_HPP

#include <autobahn/autobahn.hpp>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
namespace autobahn {
/**
 * \brief a class that wraps initialization of a raw tcp socket and WAMP client session utilizing it
 */
class wamp_tcp_client
{
public:
    using wamp_tcp_session_t = autobahn::wamp_session<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket>;
    wamp_tcp_client(std::shared_ptr<boost::asio::io_service> Io,
                    const boost::asio::ip::tcp::endpoint Endpoint,
                    const std::string &Realm,
                    bool Debug = false)
        : m_rawsocket_endpoint(Endpoint)
        , m_socket(*Io)
        , m_session(*Io, m_socket, m_socket, Debug)
        , m_realm(Realm)
    {
        m_socket.open(boost::asio::ip::tcp::v4());
        m_socket.set_option(boost::asio::ip::tcp::no_delay(true));
        m_session.m_onRxError.connect(boost::bind(&wamp_tcp_client::handleRxError,
                                                  this,
                                                  boost::asio::placeholders::error));
    }

    wamp_tcp_client(std::shared_ptr<boost::asio::io_service> Io,
                    std::string IpAddress,
                    uint16_t Port,
                    const std::string &Realm,
                    bool Debug = false)
        : wamp_tcp_client(Io,
                          boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(IpAddress), Port),
                          Realm,
                          Debug)
    {}
    /**
    * \brief launches the session asynchronously, returns a future containing an error code (0 means success)
    */
    boost::future<uint64_t> launch()
    {
        m_socket.async_connect(m_rawsocket_endpoint, [&](const boost::system::error_code& ec) {
            if (!ec) {
                m_start_future = m_session.start().then([&](boost::future<bool> started) {
                    if(started.get()){
                        m_join_future = m_session.join(m_realm).then([&](boost::future<uint64_t> session) {
                            m_session_id_promise.set_value(session.get());
                            m_connected();
                        });
                    }else{
                        m_session_id_promise.set_exception(std::runtime_error("start failed"));
                    }
                });
            } else {
                m_session_id_promise.set_exception(std::runtime_error("connect failed: " + ec.message()));
            }
        });
        return m_session_id_promise.get_future();
    }

    ~wamp_tcp_client()
    {
    }

    /**
     * This operator is used to pass through calls to the WAMP session. i.e client->call(...)
     */
    wamp_tcp_session_t* operator->()
    {
        return &m_session;
    }

    const wamp_tcp_session_t* operator->() const
    {
        return &m_session;
    }

    boost::signals2::signal<void()>& disconnected()
    {
        return m_disconnected;
    }

    boost::signals2::signal<void()>& connected()
    {
        return m_connected;
    }
    bool is_connected()
    {
        return m_session.is_connected();
    }
    private:
    void handleRxError(const boost::system::error_code &Code)
    {
        m_disconnected();
    }
    //signals emitted
    boost::signals2::signal<void()> m_disconnected;
    boost::signals2::signal<void()> m_connected;
    boost::future<void> m_start_future; ///<holds the future of the start() operation
    boost::future<void> m_join_future; ///<holds the future of the join() operation
    boost::promise<uint64_t> m_session_id_promise; ///<holds the future state of the success of launch
    boost::asio::ip::tcp::endpoint m_rawsocket_endpoint;
    boost::asio::ip::tcp::socket m_socket;
    wamp_tcp_session_t m_session; //<need to be sure this is destructed before m_pSocket
    std::string m_realm;
};
} //namespace autobahn
#endif //AUTOBAHN_TCP_CLIENT_H
