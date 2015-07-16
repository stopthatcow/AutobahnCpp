/**
  * @brief contains implemantation for class CDomainInfo
  * @file CDomainInfo.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/
#include "CDomainInfo.hpp"
#include <sstream>

namespace airware {
namespace acs {
const uint8_t CDomainInfo::PROTOCOL_VERSION_NUMBER=1;

std::string CDomainInfo::wampIp() const {
    return m_wampIp;
}

uint16_t CDomainInfo::wampPort() const {
    return m_wampPort;
}

std::string CDomainInfo::domainName() const {
    return m_domainName;
}

std::string CDomainInfo::realmName() const {
    return m_realmName;
}

void CDomainInfo::wampIp(const std::string &WampIp) {
    m_wampIp = WampIp;
}

void CDomainInfo::wampPort(const uint16_t &WampPort) {
    m_wampPort = WampPort;
}

void CDomainInfo::domainName(const std::string &DomainName) {
    m_domainName = DomainName;
}

void CDomainInfo::realmName(const std::string &RealmName) {
    m_realmName = RealmName;
}

std::string CDomainInfo::fullyQuallifiedName() const {
    return m_realmName + '/' + m_domainName;
}

std::string CDomainInfo::toString() const {
    std::stringstream stream;
    stream << fullyQuallifiedName() << '@' << m_wampIp << ':' << m_wampPort;
    return stream.str();
}
} /*namespace acs*/
} /*namespace airware*/
