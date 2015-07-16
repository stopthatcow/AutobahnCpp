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
            //TODO: see if we can post this work to the io service via dispatch()
            std::thread([&callFuture, invocation]{
                std::cerr<< "Called" << std::endl;
                autobahn::wamp_call_result result = callFuture.get();
                invocation->result(result.arguments<std::list<msgpack::object>>(),
                                   result.kw_arguments<std::unordered_map<std::string, msgpack::object>>());
                std::cerr<< "sent proxy reply" << std::endl;
                //TODO: Send back error if one was present
            }).detach();
        }else{
            invocation->error("target domain is unknown");
        }
    }

    boost::future<void> launchClient(std::string DomainName, std::shared_ptr<autobahn::wamp_tcp_client> NewClient) {
        return NewClient->launch().then([&, DomainName](boost::future <bool> connected) {
            std::string prefix = DomainName + '/';
            std::cerr << "connectOk:" << connected.get() << std::endl;
            autobahn::provide_options opts = {std::make_pair("match", msgpack::object("prefix"))};
            (*m_localClient)->provide(prefix, boost::bind(&CBackplane::proxy, this, _1), opts).wait();
            std::cerr << "local client now providing services targeting \"" << prefix << '\"' << std::endl;
        });
    }

    void connectToNewDomain(const CDomainInfo& NewDomain){
        sessionMap_t::iterator itt = m_sessionMap.find(NewDomain.domainName());
        if(itt == m_sessionMap.end()){
            //create new client
            std::cerr<< "Connecting to "<<NewDomain.wampIp()<<':'<<NewDomain.wampPort()<<std::endl;
            auto remoteClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, NewDomain.wampIp(), NewDomain.wampPort(), "default", false);
            boost::future<void> connectFuture = launchClient(NewDomain.domainName(), remoteClient);
            m_sessionMap[NewDomain.domainName()] = {remoteClient, std::move(connectFuture)};
        }
    }

    int main(int argc, char** argv){
        try {
            auto parameters = get_parameters(argc, argv);
            // create local WAMP session
            m_localClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, parameters->rawsocket_endpoint(), "default", parameters->debug());

            CServiceDiscoveryAnnouncer announcer(m_io, "239.0.0.1", 10984, parameters->rawsocket_endpoint().port(), parameters->domain(), "testRealm");
            CServiceDiscoveryListener listener(m_io, "239.0.0.1", 10984);
            listener.m_onServiceDiscovery.connect(boost::bind(&CBackplane::connectToNewDomain, this, _1));

            auto connectFuture = launchClient(parameters->domain(), m_localClient).then([&](boost::future<void>){
                //now we are ready to start service discovery:
                announcer.launch();
                listener.launch();
            });
            m_sessionMap[parameters->domain()] = {m_localClient, std::move(connectFuture)};

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

