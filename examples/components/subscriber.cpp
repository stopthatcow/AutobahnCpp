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

#include "../parameters.hpp"

#include <autobahn/autobahn.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>

void topic1(const autobahn::wamp_event& event)
{
    std::cerr << "received event: " << event->argument<std::string>(0) << std::endl;
}

int main(int argc, char** argv)
{
    try {
        boost::asio::io_service io_service;
        auto parameters = get_parameters(argc, argv);
        auto endpoint = parameters->rawsocket_endpoint();
        auto realm = parameters->realm();
        auto debug = parameters->debug();

        std::unique_ptr<autobahn::wamp_tcp_component> component(
                new autobahn::wamp_tcp_component(
                        io_service, endpoint, realm, debug));

        auto started = component->start().then(
            boost::launch::any, [&](boost::future<void>) {
            auto session = component->session();
            session->subscribe("com.examples.subscriptions.topic1", &topic1);
        });

        io_service.run();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}