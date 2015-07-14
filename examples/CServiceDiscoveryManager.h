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

class CServiceDiscoveryAnnouncer {
public:
    CServiceDiscoveryAnnouncer(std::shared_ptr<boost::asio::io_service> IoService,
                               const std::string &MulticastAddress,
                               uint16_t MulticastPort,
                               uint16_t AdvertisePort,
                               boost::posix_time::millisec Period = boost::posix_time::millisec(2000U)) :
            m_resolver(*IoService),
            m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
            m_socket(*IoService, m_endpoint.protocol()),
            m_timer(*IoService),
            m_broadcastPeriod(Period),
            m_advertisePort(AdvertisePort),
            m_messageCount(0U)
    {
        sendToAllInterfaces(boost::system::error_code()); //send the first heartbeat
    }

    void sendToAllInterfaces(const boost::system::error_code &Error) {
        if (!Error){
            boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), boost::asio::ip::host_name(), "");
            m_resolver.async_resolve(query,
                                     boost::bind(&CServiceDiscoveryAnnouncer::handleResolve, this,
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
                ++m_messageCount;
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

                m_socket.set_option(boost::asio::ip::multicast::enable_loopback(true));
                m_socket.set_option(boost::asio::ip::multicast::outbound_interface(
                        m_outgoingAnnouncements.front().to_v4()));
                m_outgoingAnnouncements.pop_front();

                m_socket.async_send_to(
                        boost::asio::buffer(m_txBuffer.data(), m_txBuffer.size()), m_endpoint,
                        boost::bind(&CServiceDiscoveryAnnouncer::sendHeartBeat, this,
                                    boost::asio::placeholders::error));
            } else {
                m_timer.expires_from_now(m_broadcastPeriod);
                m_timer.async_wait(
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
     * Holds the undpoint address and port for m_socket
     */
    boost::asio::ip::udp::endpoint m_endpoint;
    /**
     * a socket that is used to send heatbeats (note that this instance is reused for all network interfaces)
     */
    boost::asio::ip::udp::socket m_socket;
    /**
     * Holds a list of local IPs of local interfaces used for sending heartbeats
     */
    std::deque<boost::asio::ip::address> m_outgoingAnnouncements;
    /**
     * used to pace announcement by the m_broadcastPeriod interval
     */
    boost::asio::deadline_timer m_timer;
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
    size_t m_messageCount;
};


class CServiceDiscoveryListener {
public:
    CServiceDiscoveryListener(std::shared_ptr<boost::asio::io_service> IoService,
                              const std::string &MulticastAddress,
                              uint16_t MulticastPort) :
            m_endpoint(boost::asio::ip::address::from_string(MulticastAddress), MulticastPort),
            m_socket(*IoService, m_endpoint.protocol()),
            m_messageCount(0U) {
        m_socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        m_socket.bind(m_endpoint);
        //TODO: join group on all interfaces
        m_socket.set_option(boost::asio::ip::multicast::join_group(m_endpoint.address().to_v4(), boost::asio::ip::address::from_string("172.20.10.4").to_v4()));
        m_socket.set_option(boost::asio::ip::multicast::join_group(m_endpoint.address().to_v4(), boost::asio::ip::address::from_string("10.0.1.13").to_v4()));
        //m_socket.set_option(boost::asio::ip::multicast::join_group(m_endpoint.address()));
        queueReceive(boost::system::error_code());
    }
//    boost::signals2::signal<void, CServiceInfo> m_onServiceDiscovery;
private:
    void queueReceive(const boost::system::error_code &Error){
        m_socket.async_receive_from(
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
            ++m_messageCount;
        }else {
            //TODO: log error
        }
        queueReceive(boost::system::error_code());
    }

    boost::asio::ip::udp::endpoint m_endpoint;
    /**
     * a socket that is used to receive heartbeats
     */
    boost::asio::ip::udp::socket m_socket;

    std::array<char, 512> m_rxBuffer;

    boost::asio::ip::udp::endpoint m_rxEndpoint;
    /**
     * The count of received messages
     */
    size_t m_messageCount;
};
#endif //CSERVICE_DISCOVERY_ANNOUNCER_H
