/**
  * @brief contains declaration for class CDomainInfo
  * @file CDomainInfo.hpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#ifndef AUTOBAHN_CPP_CDOMAININFO_HPP
#define AUTOBAHN_CPP_CDOMAININFO_HPP

#include <stdint.h>
#include <string>
#include <msgpack.hpp>

namespace airware{
namespace acs{

class CDomainInfo{
public:
    static const uint8_t PROTOCOL_VERSION_NUMBER;
    std::string wampIp() const;
    uint16_t wampPort() const;
    std::string domainName() const;
    std::string realmName() const;
    void wampIp(const std::string &WampIp);
    void wampPort(const uint16_t &WampPort);
    void domainName(const std::string &DomainName);
    void realmName(const std::string &RealmName);
    std::string fullyQuallifiedName() const;
    std::string toString() const;
    MSGPACK_DEFINE(m_wampIp, m_wampPort, m_domainName, m_realmName);
private:
    std::string m_wampIp;
    uint16_t m_wampPort;
    std::string m_domainName;
    std::string m_realmName;
};
} /*namespace acs*/
} /*namespace airware*/

#endif //AUTOBAHN_CPP_CDOMAININFO_HPP
