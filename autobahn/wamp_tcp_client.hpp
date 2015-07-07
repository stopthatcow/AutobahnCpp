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

#ifndef AUTOBAHN_TCP_CLIENT_H
#define AUTOBAHN_TCP_CLIENT_H

#include <autobahn/autobahn.hpp>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
namespace autobahn {
/**
 * @brief a class that wraps initialization of a raw tcp socket and WAMP client session utilizing it
 */
    class wamp_tcp_client {
        typedef autobahn::wamp_session<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> wamp_tcp_session_t;
    public:
        wamp_tcp_client(std::shared_ptr<boost::asio::io_service> pIo,
                        const boost::asio::ip::tcp::endpoint &Endpoint,
                        const std::string &Realm,
                        bool Debug = false) :
                m_realm(Realm),
                m_rawsocket_endpoint(Endpoint),
                m_pIo(pIo),
                m_debug(Debug) {
            init();
        }

        wamp_tcp_client(std::shared_ptr<boost::asio::io_service> pIo,
                        const std::string &IpAddress,
                        uint16_t Port,
                        const std::string &Realm,
                        bool Debug = false) :
                m_realm(Realm),
                m_rawsocket_endpoint(boost::asio::ip::address::from_string(IpAddress), Port),
                m_pIo(pIo),
                m_debug(Debug) {
            init();
        }

        /**
     * @brief initialize members, creating shared pointers for the socket, and session
     */
        void init() {
            m_pSession.reset();
            m_pSocket.reset();
            m_connected = boost::promise<bool>();
            m_pSocket = std::make_shared<boost::asio::ip::tcp::socket>(*m_pIo);
            m_pSession = std::make_shared<wamp_tcp_session_t>(*m_pIo, *m_pSocket, *m_pSocket, m_debug);
        }

        /**
     * @brief launches the session asynchronously, returns a future containing an error code (0 means success)
     */
        boost::future<bool> launch() {
            m_pSocket->async_connect(m_rawsocket_endpoint, [&](boost::system::error_code ec) {
                if (!ec) {
                    m_startFuture = m_pSession->start().then([&](boost::future<bool> started) {
                        m_joinFuture = m_pSession->join(m_realm).then([&](boost::future<uint64_t> connected) {
                            m_connected.set_value(true);
                        });
                    });
                } else {
                    std::cerr << "connect failed: " << ec.message() << std::endl;
                    m_connected.set_value(false);
                }
            });
            return m_connected.get_future();
        }

        ~wamp_tcp_client() {
        }

        /**
     * This operator is used to pass through calls to the WAMP session. i.e *(pClient)->call(...)
     */
        std::shared_ptr<wamp_tcp_session_t> &operator->() {
            return m_pSession;
        }

    private:
        boost::future<void> m_startFuture; ///<holds the future of the start() operation
        boost::future<void> m_joinFuture; ///<holds the future of the join() operation
        boost::promise<bool> m_connected; ///<holds the future state of the success of launch
        std::shared_ptr<wamp_tcp_session_t> m_pSession; //<need to be sure this is destructed before m_pSocket
        std::shared_ptr<boost::asio::ip::tcp::socket> m_pSocket;
        std::string m_realm;
        boost::asio::ip::tcp::endpoint m_rawsocket_endpoint;
        std::shared_ptr<boost::asio::io_service> m_pIo; ///<hold a reference so that we can clean up when the last user goes out of scope
        bool m_debug;
    };
} //namespace autobahn
#endif //AUTOBAHN_TCP_CLIENT_H
