/**
  * @brief contains implemenatation for class CServiceDiscoveryManager
  * @file CServiceDiscoveryManager.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#include "acs/CServiceDiscoveryManager.hpp"

namespace airware {
namespace acs {
const uint8_t CServiceDiscoveryAnnouncer::PROTOCOL_VERSION_NUMBER=1;
CServiceDiscoveryAnnouncer::CServiceDiscoveryAnnouncer(std::shared_ptr<boost::asio::io_service> IoService,
                                                       const std::string &MulticastAddress,
                                                       uint16_t MulticastPort,
                                                       uint16_t AdvertisePort,
                                                       std::string DomainName,
                                                       std::string RealmName,
                                                       boost::posix_time::millisec Period) :
    m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
    m_txSocket(*IoService, m_endpoint.protocol()),
    m_txTimer(*IoService),
    m_broadcastPeriod(Period),
    m_txMessageCount(0U)
{
    m_domainInfo.domainName(DomainName);
    m_domainInfo.realmName(RealmName);
    m_domainInfo.wampPort(AdvertisePort);
}

void CServiceDiscoveryAnnouncer::launch() {
    sendToAllInterfaces(boost::system::error_code()); //send the first heartbeat
}

void CServiceDiscoveryAnnouncer::sendToAllInterfaces(const boost::system::error_code &Error) {
    if (!Error) {
        bool startAnnouncements = m_outgoingAnnouncements.empty();
        interfaceAddressSet_t ifcIps;
        getInterfaceAddresses(&ifcIps);
        for (interfaceAddressSet_t::const_iterator itt = ifcIps.cbegin(); itt != ifcIps.cend(); ++itt) {
            m_outgoingAnnouncements.push_back(*itt);
            //here we are looping over all known network adaptors, queueing an announcement to each
        }
        if (startAnnouncements) {
            //if there was an empty queue, kick off the async send operation
            sendHeartBeat(Error);
        }
    } else {
        std::cerr << "sendToAllInterfaces error " << Error << std::endl;
    }
}

void CServiceDiscoveryAnnouncer::sendHeartBeat(const boost::system::error_code &Error) {
    if (Error) {
        std::cerr << "sendHeartBeat error: "<< Error.message() << Error << std::endl;
    }
    bool sent = false;
    if (m_outgoingAnnouncements.size()) {
        m_txBuffer.clear();
        // serializes multiple objects into one message containing a map using msgpack::packer.
        ++m_txMessageCount;
        msgpack::packer<msgpack::sbuffer> packer(&m_txBuffer);
        m_domainInfo.wampIp(m_outgoingAnnouncements.front().to_v4().to_string());
        packer.pack(CDomainInfo::PROTOCOL_VERSION_NUMBER);
        packer.pack(m_domainInfo);

        try {
            m_txSocket.set_option(boost::asio::ip::multicast::enable_loopback(true));
            m_txSocket.set_option(
                        boost::asio::ip::multicast::outbound_interface(
                            m_outgoingAnnouncements.front().to_v4()));
            m_txSocket.async_send_to(
                        boost::asio::buffer(m_txBuffer.data(), m_txBuffer.size()), m_endpoint,
                        boost::bind(&CServiceDiscoveryAnnouncer::sendHeartBeat, this,
                                    boost::asio::placeholders::error));
            sent = true;
        } catch (const std::exception &e) {
            std::cerr << "Couldn't set outbound interface" << std::endl;
        }

        m_outgoingAnnouncements.pop_front();

    }
    if (!sent) {
        m_txTimer.expires_from_now(m_broadcastPeriod);
        m_txTimer.async_wait(
                    boost::bind(&CServiceDiscoveryAnnouncer::sendToAllInterfaces, this,
                                boost::asio::placeholders::error));
    }
}


CServiceDiscoveryListener::CServiceDiscoveryListener(std::shared_ptr<boost::asio::io_service> IoService,
                          const std::string &MulticastAddress,
                          uint16_t MulticastPort) :
    m_interfaceChangeNotifier(*IoService),
    m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
    m_rxSocket(*IoService, m_endpoint.protocol()),
    m_rxMessageCount(0U) {
    m_rxSocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    m_rxSocket.bind(m_endpoint);
    //hook up join group to interface discovery logic
    m_interfaceChangeNotifier.m_onNewInterface.connect(
                boost::bind(&CServiceDiscoveryListener::joinGroup, this, _1));
}

void CServiceDiscoveryListener::launch() {
    m_interfaceChangeNotifier.launch();
    queueReceive(boost::system::error_code());
}

void CServiceDiscoveryListener::joinGroup(const boost::asio::ip::address &IfcAddress) {
    try {
        m_rxSocket.set_option(boost::asio::ip::multicast::join_group(m_endpoint.address().to_v4(),
                                                                     IfcAddress.to_v4()));
    } catch (const std::exception &e) {
        std::cerr << "Couldn't join group" << std::endl;
    }
}

void CServiceDiscoveryListener::queueReceive(const boost::system::error_code &Error) {
    if (!Error) {
        m_rxSocket.async_receive_from(
                    boost::asio::buffer(m_rxBuffer), m_rxEndpoint,
                    boost::bind(&CServiceDiscoveryListener::onReceive, this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
    } else {
        std::cerr << "queueReceive error " << Error << std::endl;
    }
}

void CServiceDiscoveryListener::onReceive(const boost::system::error_code &Error, size_t BytesReceived) {
    if (!Error) {
        //unpack the message
        size_t offset = 0;
        msgpack::unpacked version = msgpack::unpack(m_rxBuffer.data(), BytesReceived, offset);
        if (version.get().type == msgpack::type::POSITIVE_INTEGER &&
                version.get().as<uint8_t>() == CServiceDiscoveryAnnouncer::PROTOCOL_VERSION_NUMBER) {
            msgpack::unpacked body = msgpack::unpack(m_rxBuffer.data(), BytesReceived, offset);
            std::cout << body.get() << std::endl;
            ++m_rxMessageCount;
            CDomainInfo thisDomain;
            body.get().convert(&thisDomain);
            //broadcast to listeners
            const std::string fullyQuallifiedName = thisDomain.fullyQuallifiedName();
            domainMap_t::iterator itt = m_knownDomains.find(fullyQuallifiedName);
            if (itt == m_knownDomains.end()) {
                m_knownDomains[fullyQuallifiedName] = thisDomain;
                m_onServiceDiscovery(thisDomain);
            }
        } else {
            std::cerr << "onReceive got bad packet " << Error << std::endl;
        }
    } else {
        std::cerr << "onReceive error " << Error << std::endl;
    }
    queueReceive(boost::system::error_code());
}

} /*namespace acs*/
} /*namespace airware*/
