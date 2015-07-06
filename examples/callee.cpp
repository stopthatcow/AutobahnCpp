///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2014 Tavendo GmbH
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////
#include "parameters.hpp"

#include <autobahn/autobahn.hpp>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <future>

void add(autobahn::wamp_invocation invocation)
{
    auto a = invocation->argument<uint64_t>(0);
    auto b = invocation->argument<uint64_t>(1);

    invocation->result(std::make_tuple(a + b));
}

std::shared_ptr<autobahn::wamp_session<boost::asio::ip::tcp::socket,
        boost::asio::ip::tcp::socket>> gSession;

std::vector<std::future<void>> calls;

void proxy(autobahn::wamp_invocation invocation)
{
    calls.push_back(std::move( std::async(std::launch::async, [invocation]{
        std::cerr<< "Hello from proxy" << std::endl;

        std::string procedure = "com.examples.calculator.add";//invocation->detail<std::string>("procedure");
        std::cerr<< "Calling " << procedure << std::endl;
        //TODO lookup session to use
        boost::future<autobahn::wamp_call_result> callFuture = gSession->call(procedure,
                                                                              invocation->arguments<std::list<msgpack::object>>(),
                                                                              invocation->kw_arguments<std::unordered_map<std::string, msgpack::object>>());
        std::cerr<< "Called" << std::endl;
        autobahn::wamp_call_result result = callFuture.get();
        std::cerr<< "Results" << std::endl;
        invocation->result(result.arguments<std::list<msgpack::object>>(),
                           result.kw_arguments<std::unordered_map<std::string, msgpack::object>>());
        std::cerr<< "sent proxy reply" << std::endl;
        //TODO: Send back error if one was present
    })));
    std::cerr<< "Done with proxy invocation"<<std::endl;
}

int main(int argc, char** argv)
{
    std::cerr << "Boost: " << BOOST_VERSION << std::endl;

    try {
        auto parameters = get_parameters(argc, argv);

        boost::asio::io_service io;
        boost::asio::ip::tcp::socket socket(io);

        // create a WAMP session that talks over TCP
        //
        bool debug = parameters->debug();
        auto session = std::make_shared<
                autobahn::wamp_session<boost::asio::ip::tcp::socket,
                boost::asio::ip::tcp::socket>>(io, socket, socket, debug);
        gSession = session;
        // Make sure the continuation futures we use do not run out of scope prematurely.
        // Since we are only using one thread here this can cause the io service to block
        // as a future generated by a continuation will block waiting for its promise to be
        // fulfilled when it goes out of scope. This would prevent the session from receiving
        // responses from the router.
        boost::future<void> start_future;
        boost::future<void> join_future;

        socket.async_connect(parameters->rawsocket_endpoint(),
            [&](boost::system::error_code ec) {

                if (!ec) {
                    std::cerr << "connected to server" << std::endl;

                    start_future = session->start().then([&](boost::future<bool> started) {
                        std::cerr << "session started" << std::endl;
                        join_future = session->join(parameters->realm()).then([&](boost::future<uint64_t> s) {
                            std::cerr << "joined realm: " << s.get() << std::endl;
                            session->provide("com.examples.calculator.add", &add);
                            session->provide("com.examples.proxy", &proxy);
                        });
                    });
                } else {
                    std::cerr << "connect failed: " << ec.message() << std::endl;
                }
            }
        );

        std::cerr << "starting io service" << std::endl;
        io.run();
        std::cerr << "stopped io service" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
