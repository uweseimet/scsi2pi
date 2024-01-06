//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cstring>

#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <netdb.h>
#include <unistd.h>
#include "network_util.h"

using namespace std;

bool network_util::IsInterfaceUp(const string &interface)
{
    ifreq ifr = { };
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    const int fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP);

    if (!ioctl(fd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP)) {
        close(fd);
        return true;
    }

    close(fd);
    return false;
}

vector<uint8_t> network_util::GetMacAddress(const string &interface)
{
#ifdef __linux__
    ifreq ifr = { };
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    const int fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP);

    if (!ioctl(fd, SIOCGIFHWADDR, &ifr)) {
        close(fd);
        return vector<uint8_t>(ifr.ifr_hwaddr.sa_data, ifr.ifr_hwaddr.sa_data + 6);
    }

    close(fd);
#endif

    return vector<uint8_t>();
}

set<string, less<>> network_util::GetNetworkInterfaces()
{
    set<string, less<>> network_interfaces;

#ifdef __linux__
    ifaddrs *addrs;
    getifaddrs(&addrs);
    ifaddrs *tmp = addrs;

    while (tmp) {
        if (const string name = tmp->ifa_name; tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
            !(tmp->ifa_flags & IFF_LOOPBACK)
            && (name.starts_with("eth") || name.starts_with("en") || name.starts_with("wlan"))
            && IsInterfaceUp(name)) {
            // Only list interfaces that are up
            network_interfaces.insert(name);
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
#endif

    return network_interfaces;
}

bool network_util::ResolveHostName(const string &host, sockaddr_in *addr)
{
    addrinfo hints = { };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (addrinfo *result; !getaddrinfo(host.c_str(), nullptr, &hints, &result)) {
        *addr = *reinterpret_cast<sockaddr_in*>(result->ai_addr); // NOSONAR bit_cast is not supported by the bullseye compiler
        freeaddrinfo(result);
        return true;
    }

    return false;
}
