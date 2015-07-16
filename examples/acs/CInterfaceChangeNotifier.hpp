/**
  * @brief contains declaration for class CInterfaceChangeNotifier
  * @file CInterfaceChangeNotifier.hpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/
#ifndef AUTOBAHN_CPP_CINTERFACECHANGENOTIFIER_H
#define AUTOBAHN_CPP_CINTERFACECHANGENOTIFIER_H

#include "boost/asio.hpp"
#include "boost/date_time.hpp"
#include "boost/signals2.hpp"
#include "acs/getInterfaceAddresses.hpp"
namespace airware {
namespace acs {

/**
 * This class monitors the list of network interfaces and emits a signal when a new one is detected
 */
class CInterfaceChangeNotifier {
public:
    CInterfaceChangeNotifier(boost::asio::io_service &IoService,
                             boost::posix_time::millisec Period_MS = boost::posix_time::millisec(2000U));

    void launch();

    void resolveInterfaces();

    boost::signals2::signal<void(const boost::asio::ip::address &)> m_onNewInterface;
private:
    interfaceAddressSet_t m_knownInterfaceCache;
    /**
     * The timer used to periodically update the interface list
     */
    boost::asio::deadline_timer m_timer;
    /**
     * This is the period between querries for new interfaces
     */
    boost::posix_time::millisec m_period_MS;
};
} /*namespace acs*/
} /*namespace airware*/
#endif //AUTOBAHN_CPP_CINTERFACECHANGENOTIFIER_H
