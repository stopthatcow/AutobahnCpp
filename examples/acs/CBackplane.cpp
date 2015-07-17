/**
  * @brief contains implemenatation for class CBackplane
  * @file CBackplane.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#include "acs/CBackplane.hpp"
#include <boost/algorithm/string.hpp>

namespace airware{
namespace acs{
CBackplane::CBackplane() {
    m_io = std::make_shared<boost::asio::io_service>();
}

void CBackplane::proxy(autobahn::wamp_invocation invocation) {
    std::string procedure = "NOT SPECIFIED";
    try {
        procedure = invocation->detail<std::string>("procedure");
    } catch (std::exception &e) {
        std::cerr << "No procedure specified" << std::endl;
        invocation->error(e.what());
        return;
    }
    std::vector<std::string> procedureComponents;
    boost::split(procedureComponents, procedure, boost::is_any_of("/"), boost::token_compress_on);
    if(procedureComponents.size()==2U){
        const std::string &domainName = procedureComponents[0];
        const std::string &procedureName = procedureComponents[1];
        sessionMap_t::iterator itt = m_sessionMap.find(domainName);

        if (itt != m_sessionMap.end() && itt->second.first->isConnected()) {
            std::cerr << "Calling " << procedureName << " on " << domainName << std::endl;
            boost::future<autobahn::wamp_call_result> callFuture = (*(itt->second.first))->call(procedureName,
                                                                                                invocation->arguments<std::list<msgpack::object>>(),
                                                                                                invocation->kw_arguments<std::unordered_map<std::string, msgpack::object>>());
            std::thread([&callFuture, invocation] {
                std::cerr << "Called" << std::endl;
                autobahn::wamp_call_result result = callFuture.get();
                invocation->result(result.arguments<std::list<msgpack::object>>(),
                                   result.kw_arguments<std::unordered_map<std::string, msgpack::object>>());
                std::cerr << "sent proxy reply" << std::endl;
                //TODO: Send back error if one was present (seemingly not currently supported by autobahnCpp)
            }).detach();
        } else {
            invocation->error("target domain is unknown");
        }
    }else{
        invocation->error("no domain specified");
    }
}

boost::future<void> CBackplane::launchClient(std::string DomainName,
                                             std::shared_ptr<autobahn::wamp_tcp_client> NewClient) {
    return NewClient->launch().then([&, DomainName, NewClient](boost::future<bool> connected) {
        std::string prefix = DomainName + '/';
        bool connectSuccess = connected.get();
        std::cerr << "connectOk:" << connected.get() << std::endl;
        if(connectSuccess){
            autobahn::provide_options opts = {std::make_pair("match", msgpack::object("prefix"))};
            (*m_localClient)->provide(prefix, boost::bind(&CBackplane::proxy, this, _1), opts).wait();
            std::cerr << "local client now providing services targeting \"" << prefix << '\"' << std::endl;
            m_sessionMap[DomainName].first = NewClient;
        }else{
            //TODO: need to remove NewClient from the session map
        }
    });
}

void CBackplane::connectToNewDomain(const CDomainInfo &NewDomain) {
    sessionMap_t::iterator itt = m_sessionMap.find(NewDomain.domainName());
    if (itt == m_sessionMap.end()) {
        //create new client
        std::cerr << "Connecting to " << NewDomain.wampIp() << ':' << NewDomain.wampPort() << std::endl;
        auto remoteClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, NewDomain.wampIp(),
                                                                        NewDomain.wampPort(), "default",
                                                                        false);
        boost::future<void> connectFuture = launchClient(NewDomain.domainName(), remoteClient);
        m_sessionMap[NewDomain.domainName()] = {std::move(remoteClient), std::move(connectFuture)};
    }
}

int CBackplane::main(int argc, char **argv) {
    try {
        auto parameters = get_parameters(argc, argv);
        // create local WAMP session
        m_localClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, parameters->rawsocket_endpoint(),
                                                                    "default", parameters->debug());

        CServiceDiscoveryAnnouncer announcer(m_io, "239.0.0.1", 10984,
                                             parameters->rawsocket_endpoint().port(), parameters->domain(),
                                             "testRealm");
        CServiceDiscoveryListener listener(m_io, "239.0.0.1", 10984);
        listener.m_onServiceDiscovery.connect(boost::bind(&CBackplane::connectToNewDomain, this, _1));

        auto connectFuture = launchClient(parameters->domain(), m_localClient).then([&](boost::future<void>) {
                //now we are ready to start service discovery:
                announcer.launch();
                listener.launch();
        });
        m_sessionMap[parameters->domain()] = {m_localClient, std::move(connectFuture)};

        std::cerr << "starting io service" << std::endl;
        m_io->run();
        std::cerr << "stopped io service" << std::endl;
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}

} /*namespace acs*/
} /*namespace airware*/
