/**
  * @brief contains declaration for getInterfaceAddresses
  * @file getInterfaceAddresses.hpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#ifndef AUTOBAHN_CPP_GETINTERFACEADDRESSES_H
#define AUTOBAHN_CPP_GETINTERFACEADDRESSES_H

#include <string>
#include <set>
#include "boost/asio.hpp"

/**
 * @brief type representing a set of unique IP addresses
 */
typedef std::set<boost::asio::ip::address> interfaceAddressSet_t;
/**
 * @brief populates a set of IP addresses from active interfaces
 * @param pIfcIpAddresses set to populate with interfaces from the network stack
 * @return 0 on success or OS error
 */
int getInterfaceAddresses(interfaceAddressSet_t *pIfcIpAddresses);

#endif //AUTOBAHN_CPP_GETINTERFACEADDRESSES_H
