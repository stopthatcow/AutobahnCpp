/**
  * @brief contains declaration for class CServiceDiscoveryManager
  * @file CServiceDiscoveryManager.hpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

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
#include "acs/getInterfaceAddresses.hpp"
#include "acs/CDomainInfo.hpp"
#include "acs/CInterfaceChangeNotifier.hpp"

namespace airware {
namespace acs {
class CServiceDiscoveryAnnouncer {
public:
    static const uint8_t PROTOCOL_VERSION_NUMBER;
    CServiceDiscoveryAnnouncer(std::shared_ptr<boost::asio::io_service> IoService,
                               const std::string &MulticastAddress,
                               uint16_t MulticastPort,
                               uint16_t AdvertisePort,
                               std::string DomainName,
                               std::string RealmName,
                               boost::posix_time::millisec Period = boost::posix_time::millisec(2000U));

    void launch();

    void sendToAllInterfaces(const boost::system::error_code &Error);

    void sendHeartBeat(const boost::system::error_code &Error);

private:
    /**
    * @brief Holds the endpoint address and port for m_txSocket
    */
    boost::asio::ip::udp::endpoint m_endpoint;
    /**
     * @brief the domain info that this announcer will send out
     */
    CDomainInfo m_domainInfo;
    /**
    * @brief a socket that is used to send heatbeats (note that this instance is reused for all outgoing network traffic)
    */
    boost::asio::ip::udp::socket m_txSocket;
    /**
    * @brief Holds a list of local IPs of local interfaces used for sending heartbeats
    */
    std::deque<boost::asio::ip::address> m_outgoingAnnouncements;
    /**
    * @brief used to pace announcement by the m_broadcastPeriod interval
    */
    boost::asio::deadline_timer m_txTimer;
    /**
    * @brief The period between broadcasts
    */
    boost::posix_time::millisec m_broadcastPeriod;
    /**
     * @brief buffer used to hold outgoing data
     */
    msgpack::sbuffer m_txBuffer;
    /**
    * @brief The count of transmitted messages
    */
    size_t m_txMessageCount;
};

/**
* @brief This class listens for advertised services and calls a signal when a new one is discovered
*/
class CServiceDiscoveryListener {
public:
    CServiceDiscoveryListener(std::shared_ptr<boost::asio::io_service> IoService,
                              const std::string &MulticastAddress,
                              uint16_t MulticastPort);
    /**
     * @brief queue the async loop to start
     */
    void launch();

    /**
    * @brief Signal called on discovery of new domain
    */
    boost::signals2::signal<void(const CDomainInfo &)> m_onServiceDiscovery;
private:
    void joinGroup(const boost::asio::ip::address &IfcAddress);

    void queueReceive(const boost::system::error_code &Error);

    void onReceive(const boost::system::error_code &Error, size_t BytesReceived);

    typedef std::map<std::string, CDomainInfo> domainMap_t;
    domainMap_t m_knownDomains;
    /**
    * @brief periodically notifies us of any new interfaces that we need to join IGMP group on
    */
    CInterfaceChangeNotifier m_interfaceChangeNotifier;
    /**
    * @brief the multicast endpoint that we listen to
    */
    boost::asio::ip::udp::endpoint m_endpoint;
    /**
    * @brief a socket that is used to receive heartbeats
    */
    boost::asio::ip::udp::socket m_rxSocket;
    /**
    * @brief a buffer to hold incoming serialized packets
    * @todo: more inteligently size this
    */
    std::array<char, 512U> m_rxBuffer;
    /**
    * @brief used for getting the origin of incoming packets
    */
    boost::asio::ip::udp::endpoint m_rxEndpoint;
    /**
    * @brief The count of received messages
    */
    size_t m_rxMessageCount;
};
} /*namespace acs*/
} /*namespace airware*/
#endif //CSERVICE_DISCOVERY_ANNOUNCER_H
