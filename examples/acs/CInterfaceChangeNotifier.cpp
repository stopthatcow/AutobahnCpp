/**
  * @brief contains implemenatation for class CInterfaceChangeNotifier
  * @file CInterfaceChangeNotifier.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#include "getInterfaceAddresses.hpp"
#include "CInterfaceChangeNotifier.hpp"
namespace airware {
namespace acs {

/**
 * This class monitors the list of network interfaces and emits a signal when a new one is detected
 */
CInterfaceChangeNotifier::CInterfaceChangeNotifier(boost::asio::io_service &IoService,
                                                   boost::posix_time::millisec Period_MS) :
    m_timer(IoService),
    m_period_MS(Period_MS) { }

void CInterfaceChangeNotifier::launch() {
    resolveInterfaces();
}

void CInterfaceChangeNotifier::resolveInterfaces() {
    interfaceAddressSet_t currentIfcIps;
    std::list <boost::asio::ip::address> newIfcIps;
    getInterfaceAddresses(&currentIfcIps);
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
} /*namespace acs*/
} /*namespace airware*/
