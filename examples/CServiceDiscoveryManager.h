//
// Created by Nicholas Wiles on 7/13/15.
//

#ifndef CSERVICE_DISCOVERY_ANNOUNCER_H
#define CSERVICE_DISCOVERY_ANNOUNCER_H

#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <memory>
#include <string>
#include <boost/signals2.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <stdint.h>
#include <msgpack.hpp>

static const uint8_t PROTOCOL_VERSION_NUMBER = 1U;
class CDomainInfo{
public:
    std::string wampIp() const{
        return m_wampIp;
    }
    uint16_t wampPort() const{
        return m_wampPort;
    }
    std::string domainName() const{
        return m_domainName;
    }
    std::string realmName() const{
        return m_realmName;
    }
    void wampIp(const std::string &WampIp){
        m_wampIp = WampIp;
    }
    void wampPort(const uint16_t &WampPort){
        m_wampPort = WampPort;
    }
    void domainName(const std::string &DomainName){
        m_domainName = DomainName;
    }
    void realmName(const std::string &RealmName) {
        m_realmName = RealmName;
    }
    std::string fullyQuallifiedName() const{
        return m_realmName + '/' + m_domainName;
    }
    std::string toString() const{
        std::stringstream stream;
        stream << fullyQuallifiedName() << '@' << m_wampIp << ':' << m_wampPort;
        return stream.str();
    }
    MSGPACK_DEFINE(m_wampIp, m_wampPort, m_domainName, m_realmName);
private:
    std::string m_wampIp;
    uint16_t m_wampPort;
    std::string m_domainName;
    std::string m_realmName;
};

typedef std::set<boost::asio::ip::address> interfaceSet_t;
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
int getInterfaceListPosix(interfaceSet_t *pIfcIpAddresses){
    int ret=-1;
    if(pIfcIpAddresses != NULL) {
        struct ifaddrs *ifap, *ifa;
        ret=0;
        getifaddrs(&ifap);
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr->sa_family==AF_INET) {
                if (ifa->ifa_addr) {
                    struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;
                    pIfcIpAddresses->insert(boost::asio::ip::address::from_string(inet_ntoa(sa->sin_addr)));
                }
            }
        }

        freeifaddrs(ifap);
    }
    return ret;
};
/*
int getInterfaceListWin32(interfaceSet_t *pIfcIpAddresses) {
    int ret = -1;
    if (pIfcIpAddresses) {
        // Allocate a 15 KB buffer to start with.
        outBufLen = 15 * 1024;
        DWORD dwRetVal = ERROR_BUFFER_OVERFLOW;
        std::unique_ptr<IP_ADAPTER_ADDRESSES *> pAddresses;

        for (int iteration = 0; (dwRetVal == ERROR_BUFFER_OVERFLOW) && (iteration < 3); ++iteration) {
            pAddresses.reset(new char[outBufLen]);
            dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
        }

        if (dwRetVal == NO_ERROR) {
            // If successful, output some information from the data we received

            for (IP_ADAPTER_ADDRESSES *pCurrAddresses = pAddresses.get(); pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
                pCurrAddresses->FirstUnicastAddress->Address;
                pIfcIpAddresses->insert()
                //TODO: translate this to a string?
            }
        } else {
            printf("Call to GetAdaptersAddresses failed with error: %d\n",
                   dwRetVal);
        }
    }

    return ret;
}*/


void queryInterfaces(interfaceSet_t *pIfcIps) {
    if (pIfcIps != NULL) {
        getInterfaceListPosix(pIfcIps);
    }
}
/**
 * This class monitors the list of network interfaces and emits a signal when a new one is detected
 */
class CInterfaceChangeNotifier {
public:

    CInterfaceChangeNotifier(boost::asio::io_service &IoService,
                             boost::posix_time::millisec Period_MS = boost::posix_time::millisec(2000U)) :
            m_timer(IoService),
            m_period_MS(Period_MS) { }

    void launch() {
        resolveInterfaces();
    }

    void resolveInterfaces() {
        interfaceSet_t currentIfcIps;
        std::list<boost::asio::ip::address> newIfcIps;
        queryInterfaces(&currentIfcIps);
        std::set_difference(currentIfcIps.begin(), currentIfcIps.end(),
                            m_knownInterfaceCache.begin(), m_knownInterfaceCache.end(),
                            std::inserter(newIfcIps, newIfcIps.begin()));
        for (std::list<boost::asio::ip::address>::const_iterator ipIt = newIfcIps.cbegin();
             ipIt != newIfcIps.cend(); ++ipIt) {
            boost::asio::ip::address address = *ipIt;
            std::cout << "Discovered interface:" << address.to_string() << std::endl;
            m_onNewInterface(address);
        }
        m_knownInterfaceCache = currentIfcIps;
        m_timer.expires_from_now(m_period_MS);
        m_timer.async_wait(boost::bind(&CInterfaceChangeNotifier::resolveInterfaces, this));
    }

    boost::signals2::signal<void(const boost::asio::ip::address &)> m_onNewInterface;
private:
    interfaceSet_t m_knownInterfaceCache;
    /**
 * The timer used to periodically update the interface list
 */
    boost::asio::deadline_timer m_timer;

    boost::posix_time::millisec m_period_MS;
};

class CServiceDiscoveryAnnouncer {
public:
    CServiceDiscoveryAnnouncer(std::shared_ptr<boost::asio::io_service> IoService,
                               const std::string &MulticastAddress,
                               uint16_t MulticastPort,
                               uint16_t AdvertisePort,
                               std::string DomainName,
                               std::string RealmName,
                               boost::posix_time::millisec Period = boost::posix_time::millisec(2000U)) :
            m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
            m_domainName(DomainName),
            m_realmName(RealmName),
            m_txSocket(*IoService, m_endpoint.protocol()),
            m_txTimer(*IoService),
            m_broadcastPeriod(Period),
            m_advertisePort(AdvertisePort),
            m_txMessageCount(0U) {
    }

    void launch() {
        sendToAllInterfaces(boost::system::error_code()); //send the first heartbeat
    }

    void sendToAllInterfaces(const boost::system::error_code &Error) {
        if (!Error) {
            bool startAnnouncements = m_outgoingAnnouncements.empty();
            interfaceSet_t ifcIps;
            queryInterfaces(&ifcIps);
            for (interfaceSet_t::const_iterator itt = ifcIps.cbegin(); itt != ifcIps.cend(); ++itt) {
                m_outgoingAnnouncements.push_back(*itt);
                //here we are looping over all known network adaptors, queueing an announcement to each
            }
            if (startAnnouncements) {
                //if there was an empty queue, kick off the async send operation
                sendHeartBeat(Error);
            }
        } else {
            //TODO: Log this
            std::cerr << "sendToAllInterfaces error " << Error << std::endl;
        }
    }

    void sendHeartBeat(const boost::system::error_code &Error) {
        if (Error) {
            //TODO: log this
            std::cerr << "sendHeartBeat error " << Error << std::endl;
        }
        bool sent = false;
        if (m_outgoingAnnouncements.size()) {
            m_txBuffer.clear();
            // serializes multiple objects into one message containing a map using msgpack::packer.
            ++m_txMessageCount;
            msgpack::packer<msgpack::sbuffer> packer(&m_txBuffer);
            CDomainInfo info;
            info.domainName(m_domainName);
            info.wampIp(m_outgoingAnnouncements.front().to_v4().to_string());
            info.wampPort(m_advertisePort);
            info.realmName(m_realmName);
            packer.pack(PROTOCOL_VERSION_NUMBER);
            packer.pack(info);

            try {
                m_txSocket.set_option(boost::asio::ip::multicast::enable_loopback(true));
                m_txSocket.set_option(
                        boost::asio::ip::multicast::outbound_interface(m_outgoingAnnouncements.front().to_v4()));
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

private:
    /**
 * Holds the undpoint address and port for m_txSocket
 */
    boost::asio::ip::udp::endpoint m_endpoint;

    std::string m_domainName;

    std::string m_realmName;
    /**
 * a socket that is used to send heatbeats (note that this instance is reused for all outgoing network traffic)
 */
    boost::asio::ip::udp::socket m_txSocket;
    /**
 * Holds a list of local IPs of local interfaces used for sending heartbeats
 */
    std::deque<boost::asio::ip::address> m_outgoingAnnouncements;
    /**
 * used to pace announcement by the m_broadcastPeriod interval
 */
    boost::asio::deadline_timer m_txTimer;
    /**
 * The period between broadcasts
 */
    boost::posix_time::millisec m_broadcastPeriod;

    msgpack::sbuffer m_txBuffer;
    /**
 * Port advertised for connection via heartbeat
 */
    uint16_t m_advertisePort;
    /**
 * The count of transmitted messages
 */
    size_t m_txMessageCount;
};

/**
 * This class listens for advertised services and calls a signal when a new one is discovered
 */
class CServiceDiscoveryListener {
public:
    CServiceDiscoveryListener(std::shared_ptr<boost::asio::io_service> IoService,
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

    void launch() {
        m_interfaceChangeNotifier.launch();
        queueReceive(boost::system::error_code());
    }

    /**
 * Signal called on discovery of new domain
 */
    boost::signals2::signal<void(const CDomainInfo &)> m_onServiceDiscovery;
private:
    void joinGroup(const boost::asio::ip::address &IfcAddress) {
        try {
            m_rxSocket.set_option(boost::asio::ip::multicast::join_group(m_endpoint.address().to_v4(),
                                                                         IfcAddress.to_v4()));
        } catch (const std::exception &e) {
            std::cerr << "Couldn't join group" << std::endl;
        }
    }

    void queueReceive(const boost::system::error_code &Error) {
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

    void onReceive(const boost::system::error_code &Error, size_t BytesReceived) {
        if (!Error) {
            //unpack the message
            size_t offset = 0;
            msgpack::unpacked version = msgpack::unpack(m_rxBuffer.data(), BytesReceived, offset);
            //todo: check the version #
            if (version.get().type == msgpack::type::POSITIVE_INTEGER ||
                version.get().as<uint8_t>() != PROTOCOL_VERSION_NUMBER) {
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
            } else { ; //TODO: log error
                std::cerr << "onReceive got bad packet " << Error << std::endl;
            }
        } else {
            //TODO: log error
            std::cerr << "onReceive error " << Error << std::endl;
        }
        queueReceive(boost::system::error_code());
    }

    typedef std::map<std::string, CDomainInfo> domainMap_t;
    domainMap_t m_knownDomains;
    /**
 * periodically notifies us of any new interfaces that we need to join IGMP group on
 */
    CInterfaceChangeNotifier m_interfaceChangeNotifier;
    /**
 * the multicast endpoint that we listen to
 */
    boost::asio::ip::udp::endpoint m_endpoint;
    /**
 * a socket that is used to receive heartbeats
 */
    boost::asio::ip::udp::socket m_rxSocket;
    /**
 * a buffer to hold incoming serialized packets
 */
    std::array<char, 512U> m_rxBuffer;
    /**
 * used for getting the origin of incoming packets
 */
    boost::asio::ip::udp::endpoint m_rxEndpoint;
    /**
 * The count of received messages
 */
    size_t m_rxMessageCount;
};
#endif //CSERVICE_DISCOVERY_ANNOUNCER_H
