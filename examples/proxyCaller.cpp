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


int main(int argc, char** argv)
{
    std::cerr << "Boost: " << BOOST_VERSION << std::endl;

    try {
        auto parameters = get_parameters(argc, argv);

        auto io = std::make_shared<boost::asio::io_service> ();
        auto client = std::make_shared<autobahn::wamp_tcp_client>(io,
                                                                 parameters->rawsocket_endpoint(),
                                                                 "default",
                                                                 parameters->debug());

        boost::future<void> callFuture;
        boost::future<void> startFuture;
        startFuture = client->launch().then([&](boost::future<bool> connected) {
            std::cerr << "connectOk:" << connected.get() << std::endl;
            std::tuple<uint64_t, uint64_t> arguments(23, 777);
            callFuture = (*client)->call("domain1/add", arguments).then(
                    [&](boost::future<autobahn::wamp_call_result> result) {
                        uint64_t sum = result.get().argument<uint64_t>(0);
                        std::cerr << "call result: " << sum << std::endl;
                        client.reset();
                    });
        });

        std::cerr << "starting io service" << std::endl;
        io->run();
        std::cerr << "stopped io service" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
