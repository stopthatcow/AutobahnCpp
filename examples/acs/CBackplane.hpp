/**
  * @brief contains declaration for class CBackplane
  * @file CBackplane.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#ifndef ACS_CBACKPLANE_HPP
#define ACS_CBACKPLANE_HPP

#include "acs/CServiceDiscoveryManager.hpp"
#include "parameters.hpp"
#include <autobahn/autobahn.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <future>

namespace airware {
namespace acs {
class CBackplane {
public:
    CBackplane();

    void proxy(autobahn::wamp_invocation invocation);

    boost::future<void> launchClient(std::string DomainName,
                                     std::shared_ptr<autobahn::wamp_tcp_client> NewClient);

    void connectToNewDomain(const CDomainInfo &NewDomain);

    int main(int argc, char **argv);

private:
    typedef std::pair<std::shared_ptr<autobahn::wamp_tcp_client>, boost::future<void>> mapEntry_t;
    std::shared_ptr<autobahn::wamp_tcp_client> m_localClient;
    typedef std::map<std::string, mapEntry_t> sessionMap_t;
    sessionMap_t m_sessionMap;
    std::shared_ptr<boost::asio::io_service> m_io;
};

}
}
#endif // ACS_CBACKPLANE_HPP

