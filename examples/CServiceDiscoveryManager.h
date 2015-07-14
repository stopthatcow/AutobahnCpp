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


template<typename ResolveHandler>
void queryInterfaces(boost::asio::ip::udp::resolver &Resolver, ResolveHandler Handler){
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), boost::asio::ip::host_name(), "");
    Resolver.async_resolve(query, Handler);
}

/**
 * This class monitors the list of network interfaces and emits a signal when a new one is detected
 */
class CInterfaceChangeNotifier{
public:
    void handleResolve(boost::asio::ip::udp::resolver::iterator endpoint_iterator){
        std::set<boost::asio::ip::address> newKnownInterfaces;
        for(; endpoint_iterator != boost::asio::ip::udp::resolver::iterator(); ++endpoint_iterator) {
            interfaceSet_t::iterator itt = m_knownInterfaces.find(endpoint_iterator->endpoint().address());
            if(itt == m_knownInterfaces.end()){
                //found a new interface!
                std::cout<<"New interface:"<< endpoint_iterator->endpoint().address().to_string() <<std::endl;
                m_onNewInterface( endpoint_iterator->endpoint().address() );
            }
            newKnownInterfaces.insert(endpoint_iterator->endpoint().address());
        }
        m_knownInterfaces = newKnownInterfaces;
    }
    boost::signals2::signal<void (const boost::asio::ip::address &)> m_onNewInterface;
private:
    typedef std::set<boost::asio::ip::address> interfaceSet_t;
    interfaceSet_t m_knownInterfaces;
};

class CServiceDiscoveryAnnouncer {
public:
    CServiceDiscoveryAnnouncer(std::shared_ptr<boost::asio::io_service> IoService,
                               const std::string &MulticastAddress,
                               uint16_t MulticastPort,
                               uint16_t AdvertisePort,
                               boost::posix_time::millisec Period = boost::posix_time::millisec(2000U)) :
            m_resolver(*IoService),
            m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
            m_txSocket(*IoService, m_endpoint.protocol()),
            m_txTimer(*IoService),
            m_broadcastPeriod(Period),
            m_advertisePort(AdvertisePort),
            m_txMessageCount(0U) {
        sendToAllInterfaces(boost::system::error_code()); //send the first heartbeat
    }

    void sendToAllInterfaces(const boost::system::error_code &Error) {
        if (!Error){
            //hook up
            queryInterfaces(m_resolver, boost::bind(&CServiceDiscoveryAnnouncer::handleResolve, this,
                                                    boost::asio::placeholders::error,
                                                    boost::asio::placeholders::iterator));
        }else{
            //TODO: Log this
        }
    }

    void handleResolve(const boost::system::error_code& error,
                       boost::asio::ip::udp::resolver::iterator endpoint_iterator)
    {
        if (!error)
        {
            bool startAnnouncements = m_outgoingAnnouncements.empty();
            for(; endpoint_iterator != boost::asio::ip::udp::resolver::iterator(); ++endpoint_iterator) {
                m_outgoingAnnouncements.push_back(endpoint_iterator->endpoint().address());
                //here we are looping over all known network adaptors, queueing an announcement to each
            }
            if(startAnnouncements){
                //if there was an empty queue, kick off the async send operation
                sendHeartBeat(error);
            }
        }else{
            //TODO: Log this
        }
    }

    void sendHeartBeat(const boost::system::error_code& Error) {
        if (!Error) {
            if (m_outgoingAnnouncements.size()) {
                m_txBuffer.clear();
                // serializes multiple objects into one message containing a map using msgpack::packer.
                ++m_txMessageCount;
                msgpack::packer<msgpack::sbuffer> packer(&m_txBuffer);
                packer.pack_map(4);
                packer.pack(std::string("ver"));
                packer.pack(1);
                packer.pack(std::string("ip"));
                packer.pack(m_outgoingAnnouncements.front().to_v4().to_string());
                packer.pack(std::string("port"));
                packer.pack(m_advertisePort);
                packer.pack("name");
                packer.pack("uav1");
                packer.pack("relm");
                packer.pack("test");

                m_txSocket.set_option(boost::asio::ip::multicast::enable_loopback(true));
                m_txSocket.set_option(boost::asio::ip::multicast::outbound_interface(
                        m_outgoingAnnouncements.front().to_v4()));
                m_outgoingAnnouncements.pop_front();

                m_txSocket.async_send_to(
                        boost::asio::buffer(m_txBuffer.data(), m_txBuffer.size()), m_endpoint,
                        boost::bind(&CServiceDiscoveryAnnouncer::sendHeartBeat, this,
                                    boost::asio::placeholders::error));
            } else {
                m_txTimer.expires_from_now(m_broadcastPeriod);
                m_txTimer.async_wait(
                        boost::bind(&CServiceDiscoveryAnnouncer::sendToAllInterfaces, this,
                                    boost::asio::placeholders::error));
            }
        }else{
            //TODO: log this
        }
    }

private:
    /**
     * Used to resolve network interfaces so that heartbeats go to all known interfaces
     */
    boost::asio::ip::udp::resolver m_resolver;
    /**
     * Holds the undpoint address and port for m_txSocket
     */
    boost::asio::ip::udp::endpoint m_endpoint;
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
            m_resolver(*IoService),
            m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
            m_rxSocket(*IoService, m_endpoint.protocol()),
            m_rxMessageCount(0U) {
        m_rxSocket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        m_rxSocket.bind(m_endpoint);
        queueReceive(boost::system::error_code());
        //hook up join group to interface discovery logic
        m_interfaceChangeNotifier.m_onNewInterface.connect(boost::bind(&CServiceDiscoveryListener::joinGroup, this, _1));
        queryInterfaces(m_resolver, boost::bind(&CInterfaceChangeNotifier::handleResolve, &m_interfaceChangeNotifier,
                                                boost::asio::placeholders::iterator));
    }
    //TODO: create signal for service discovery
//    boost::signals2::signal<void, CServiceInfo> m_onServiceDiscovery;
private:
    void joinGroup(const boost::asio::ip::address &IfcAddress){
        m_rxSocket.set_option(boost::asio::ip::multicast::join_group(m_endpoint.address().to_v4(), IfcAddress.to_v4()));
    }
    void queueReceive(const boost::system::error_code &Error){
        m_rxSocket.async_receive_from(
                boost::asio::buffer(m_rxBuffer), m_rxEndpoint,
                boost::bind(&CServiceDiscoveryListener::onReceive, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
    }

    void onReceive(const boost::system::error_code &Error, size_t BytesReceived){
        if(!Error){
            //unpack the message
            msgpack::unpacked msg;
            msgpack::unpack(&msg, m_rxBuffer.data(), BytesReceived);
            std::cout << msg.get() << std::endl;
            ++m_rxMessageCount;
        }else {
            //TODO: log error
        }
        queueReceive(boost::system::error_code());
    }
    /**
     * Used to resolve network interfaces so that heartbeats are received from all known interfaces
     */
    boost::asio::ip::udp::resolver m_resolver;
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
    std::array<char, 512> m_rxBuffer;
    /**
     * used for getting the origin of incoming packets
     */
    boost::asio::ip::udp::endpoint m_rxEndpoint;
    /**
     * The count of received messages
     */
    size_t m_rxMessageCount;

    CInterfaceChangeNotifier m_interfaceChangeNotifier;
};
#endif //CSERVICE_DISCOVERY_ANNOUNCER_H
