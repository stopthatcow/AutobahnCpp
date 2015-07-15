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

std::vector<std::future<void>> calls;


#include "CServiceDiscoveryManager.h"

class CBackplane {
public:
    CBackplane() {
        m_io = std::make_shared<boost::asio::io_service>();
    }

    void proxy(autobahn::wamp_invocation invocation)
    {
        std::string procedure = "NOT SPECIFIED";
        try {
            procedure = invocation->detail<std::string>("procedure");
        }catch(std::exception& e){
            std::cerr << "No procedure specified" << std::endl;
            invocation->error(e.what());
            return;
        }
        //TODO: use tokenize
        std::string domainName = procedure.substr(0, procedure.find_first_of('/'));
        std::string procedureName = procedure.substr(procedure.find_first_of('/')+1U);
        sessionMap_t::iterator itt = m_sessionMap.find(domainName);
        if(itt != m_sessionMap.end() ){
            std::cerr<< "Calling " << procedureName << " on " << domainName << std::endl;
            boost::future<autobahn::wamp_call_result> callFuture = (*(itt->second.first))->call(procedureName,
                                                                                     invocation->arguments<std::list<msgpack::object>>(),
                                                                                     invocation->kw_arguments<std::unordered_map<std::string, msgpack::object>>());

            calls.push_back(std::move( std::async(std::launch::async, [&callFuture, invocation]{
                std::cerr<< "Called" << std::endl;
                autobahn::wamp_call_result result = callFuture.get();
                invocation->result(result.arguments<std::list<msgpack::object>>(),
                                   result.kw_arguments<std::unordered_map<std::string, msgpack::object>>());
                std::cerr<< "sent proxy reply" << std::endl;
                //TODO: Send back error if one was present
            })));
        }else{
            invocation->error("target domain is unknown");
        }
    }

    void connectToNewDomain(const CDomainInfo& NewDomain){
        sessionMap_t::iterator itt = m_sessionMap.find(NewDomain.fullyQuallifiedName());
        if(itt == m_sessionMap.end()){
            //create new client
            std::cerr<< "Connecting to "<<NewDomain.wampIp()<<':'<<NewDomain.wampPort()<<std::endl;
            auto remoteClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, NewDomain.wampIp(), NewDomain.wampPort(), "default", false);
            boost::future<void> connectFuture = remoteClient->launch().then([&, NewDomain](boost::future<bool> connected){
                                                                            std::string prefix = NewDomain.domainName() + '/';
                                                                            std::cerr << "connectOk:" << connected.get() << std::endl;
                                                                            autobahn::provide_options opts = {std::make_pair("match", msgpack::object("prefix"))};
                                                                            (*m_localClient)->provide(prefix, boost::bind(&CBackplane::proxy, this, _1), opts).wait();
                                                                            std::cerr << "local client now providing services targeting \"" << prefix <<'\"'<< std::endl;
            });
            m_sessionMap[NewDomain.domainName()] = {remoteClient, std::move(connectFuture)};
        }
    }

    int main(int argc, char** argv){
        try {
            auto parameters = get_parameters(argc, argv);
            // create local WAMP session
            m_localClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, parameters->rawsocket_endpoint(), "default", parameters->debug());

            CServiceDiscoveryAnnouncer sdMgr(m_io, "239.0.0.1", 10984, parameters->rawsocket_endpoint().port(), parameters->domain(), "testRealm");
            CServiceDiscoveryListener listener(m_io, "239.0.0.1", 10984);
            listener.m_onServiceDiscovery.connect(boost::bind(&CBackplane::connectToNewDomain, this, _1));

            // Make sure the continuation futures we use do not run out of scope prematurely.
            // Since we are only using one thread here this can cause the io service to block
            // as a future generated by a continuation will block waiting for its promise to be
            // fulfilled when it goes out of scope. This would prevent the session from receiving
            // responses from the router.

            auto start_future = m_localClient->launch().then([&](boost::future<bool> connected){
                std::cerr << "connectOk:" << connected.get() << std::endl;
            });

            std::cerr << "starting io service" << std::endl;
            m_io->run();
            std::cerr << "stopped io service" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
        return 0;
    }
private:
    typedef std::pair<std::shared_ptr<autobahn::wamp_tcp_client>, boost::future<void>> mapEntry_t;
    std::shared_ptr<autobahn::wamp_tcp_client> m_localClient;
    typedef std::map<std::string, mapEntry_t > sessionMap_t;
    sessionMap_t m_sessionMap;
    std::shared_ptr<boost::asio::io_service> m_io;
};

int main(int argc, char** argv)
{
    CBackplane bp;
    return bp.main(argc, argv);
}

