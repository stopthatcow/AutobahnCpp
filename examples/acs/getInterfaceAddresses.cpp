/**
  * @brief contains implemenatation for getInterfaceAddresses
  * @file getInterfaceAddresses.cpp
  * @author nwiles
  * @copyright Copyright (c) 2015 Airware. All rights reserved.
  * @date 2015-7-16
*/

#include "getInterfaceAddresses.hpp"

#ifdef _WIN32
#include <stdio.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
int getInterfaceAddresses(interfaceAddressSet_t *pIfcIpAddresses) {
    int ret = -1;
    if (pIfcIpAddresses) {
        // Allocate a 15 KB buffer to start with.
        outBufLen = 15 * 1024;
        DWORD rawRetVal = ERROR_BUFFER_OVERFLOW;
        std::unique_ptr<IP_ADAPTER_ADDRESSES *> pAddresses;

        for (int iteration = 0; (rawRetVal == ERROR_BUFFER_OVERFLOW) && (iteration < 3); ++iteration) {
            pAddresses.reset(new char[outBufLen]);
            rawRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
        }
        ret = rawRetVal;

        if (rawRetVal == NO_ERROR) {
            ret = 0;
            for (IP_ADAPTER_ADDRESSES *pCurrAddresses = pAddresses.get(); pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
                const SOCKET_ADDRESS &address = pCurrAddresses->FirstUnicastAddress.Address;

                char ipString[NI_MAXHOST] = {0};
                ret = getnameinfo(address.lpSockaddr, address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
                if(ret==0){
                    pIfcIpAddresses->insert(boost::asio::ip::address::from_string(ipString));
                }else{
                    std::cerr<< "getnameinfo() failed:"<<ret<< std::endl;
                }
            }
        } else {
            std::cerr<< "GetAdaptersAddresses() failed:"<<ret<<std::endl;
        }
    }

    return ret;
}

#else //assume POSIX

#include <ifaddrs.h>

int getInterfaceAddresses(interfaceAddressSet_t *pIfcIpAddresses){
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
#endif
