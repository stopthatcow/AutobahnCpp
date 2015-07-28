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

        if (itt != m_sessionMap.end() && itt->second->isConnected()) {
            std::cerr << "Calling " << procedureName << " on " << domainName << std::endl;
            boost::future<autobahn::wamp_call_result> callFuture = (*(itt->second))->call(procedureName,
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
            (*m_localClient)->provide(prefix, boost::bind(proxy, NewClient, _1), opts).wait(); //TODO this needs to survive restarts
            std::cerr << "local client now providing services targeting \"" << prefix << '\"' << std::endl;
        }else{
            m_io->dispatch([&](){
                m_sessionMap.erase(DomainName);
                m_listener->erase(DomainName);
            });
        }
    });
}

void CBackplane::connectToNewDomain(const CDomainInfo &NewDomain) {
    std::string domainName = NewDomain.domainName() ;
    sessionMap_t::iterator itt = m_sessionMap.find(domainName);
    if (itt == m_sessionMap.end()) {
        //create new client
        std::cerr << "Connecting to " << NewDomain.wampIp() << ':' << NewDomain.wampPort() << std::endl;
        auto remoteClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, NewDomain.wampIp(),
                                                                        NewDomain.wampPort(), "default",
                                                                        false);
        m_sessionMap[domainName] = remoteClient;//add to list of known clients
        remoteClient->m_onDisconnect.connect([&, domainName](){ //when the client disconnects, remove it from the map
            std::cerr << "Disconnect " << domainName << std::endl;
            m_sessionMap.erase(domainName);
            m_listener->erase(domainName);
        });
        boost::future<void> connectFuture = launchClient(NewDomain.domainName(), remoteClient);
        m_outstandingFutures.push_back(std::move(connectFuture)); //TODO: periodically cull these
    }
}

//void CBackplane::handleSetNetworkPublishInterval(autobahn::wamp_invocation invocation) {
    //TODO: add to the outgoing list of subscriptions
//}

int CBackplane::main(int argc, char **argv) {
    try {
        auto parameters = get_parameters(argc, argv);
        // create local WAMP session
        m_localClient = std::make_shared<autobahn::wamp_tcp_client>(m_io, parameters->rawsocket_endpoint(),
                                                                    "default", parameters->debug());

        auto connectFuture = launchClient(parameters->domain(), m_localClient).then([&](boost::future<void>) {
                //now we are ready to start service discovery:
                m_announcer->launch();
                m_listener->launch();
               // (*m_localClient)->provide("acs.setNetworkPublishInterval", boost::bind(&CBackplane::handleSetNetworkPublishInterval this));
        });
        m_sessionMap[parameters->domain()] = m_localClient;

        //domain discovery
        m_announcer = std::make_shared<CDomainDiscoveryAnnouncer>(m_io, "239.0.0.1", 10984,
                                             parameters->rawsocket_endpoint().port(), parameters->domain(),
                                             "testRealm");
        m_listener = std::make_shared<CDomainDiscoveryListener>(m_io, "239.0.0.1", 10984);
        m_listener->m_onDomainDiscovery.connect(boost::bind(&CBackplane::connectToNewDomain, this, _1));

        //hook up the udp backplane to the local WAMP client
        //m_udpBackplane.m_onRxData.connect(boost::bind(&autobahn::wamp_session::publish, *m_localClient)).track(*m_localClient);
        //m_udpBackplane.m_onRequestSubscription.connect(boost::bind(&autobahn::wamp_session::subscribe, *m_localClient)).track(*m_localClient);
        //m_udpBackplane.m_onRequestUnsubscription.connect(boost::bind(&autobahn::wamp_session::unsubscribe, *m_localClient)).track(*m_localClient);

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
