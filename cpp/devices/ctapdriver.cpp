//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <spdlog/spdlog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sstream>
#ifdef __linux__
#include <linux/if_tun.h>
#include <linux/sockios.h>
#endif
#include "shared/s2p_util.h"
#include "shared/network_util.h"
#include "ctapdriver.h"

using namespace std;
using namespace spdlog;
using namespace s2p_util;
using namespace network_util;

static string br_setif(int br_socket_fd, const string &bridgename, const string &ifname, bool add)
{
#ifndef __linux__
	return "if_nametoindex: Linux is required";
#else
    ifreq ifr;
    ifr.ifr_ifindex = if_nametoindex(ifname.c_str());
    if (!ifr.ifr_ifindex) {
        return "Can't if_nametoindex " + ifname;
    }
    strncpy(ifr.ifr_name, bridgename.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (ioctl(br_socket_fd, add ? SIOCBRADDIF : SIOCBRDELIF, &ifr) == -1) {
        return "Can't ioctl " + string(add ? "SIOCBRADDIF" : "SIOCBRDELIF");
    }
    return "";
#endif
}

string ip_link(int fd, const char *ifname, bool up)
{
    ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1) {
        return "Can't ioctl SIOCGIFFLAGS";
    }
    ifr.ifr_flags &= ~IFF_UP;
    if (up) {
        ifr.ifr_flags |= IFF_UP;
    }
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) == -1) {
        return "Can't ioctl SIOCSIFFLAGS";
    }
    return "";
}

bool CTapDriver::Init(const param_map &const_params)
{
    param_map params = const_params;
    stringstream s(params["interface"]);
    string interface;
    while (getline(s, interface, ',')) {
        interfaces.push_back(interface);
    }
    inet = params["inet"];

    if ((tap_fd = open("/dev/net/tun", O_RDWR)) < 0) {
        LogErrno("Can't open tun");
        return false;
    }

#ifndef __linux__
    return false;
#else

    // IFF_NO_PI for no extra packet information
    ifreq ifr = { };
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, DEFAULT_BRIDGE_IF.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe

    if (const int ret = ioctl(tap_fd, TUNSETIFF, (void*)&ifr); ret == -1) {
        LogErrno("Can't ioctl TUNSETIFF");
        close(tap_fd);
        return false;
    }

    const int ip_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (ip_fd < 0) {
        LogErrno("Can't open ip socket");
        close(tap_fd);
        return false;
    }

    const int br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (br_socket_fd < 0) {
        LogErrno("Can't open bridge socket");
        close(tap_fd);
        close(ip_fd);
        return false;
    }

    auto cleanUp = [&](const string &error) {
        LogErrno(error);
        close(tap_fd);
        close(ip_fd);
        close(br_socket_fd);
        return false;
    };

    // Check if the bridge has already been created by checking whether there is a MAC address for the bridge.
    if (GetMacAddress(BRIDGE_NAME).empty()) {
        trace("Checking which interface is available for creating the bridge " + BRIDGE_NAME);

        const auto &it = ranges::find_if(interfaces, [](const string &iface) {return IsInterfaceUp(iface);});
        if (it == interfaces.end()) {
            return cleanUp("No interface is up, not creating bridge " + BRIDGE_NAME);
        }

        const string bridge_interface = *it;

        info("Creating " + BRIDGE_NAME + " for interface " + bridge_interface);

        if (bridge_interface == "eth0") {
            if (const string error = SetUpEth0(br_socket_fd, bridge_interface); !error.empty()) {
                return cleanUp(error);
            }
        }
        else if (const string error = SetUpNonEth0(br_socket_fd, ip_fd, inet); !error.empty()) {
            return cleanUp(error);
        }

        trace(">ip link set dev " + BRIDGE_NAME + " up");

        if (const string error = ip_link(ip_fd, BRIDGE_NAME.c_str(), true); !error.empty()) {
            return cleanUp(error);
        }
    }
    else {
        info(BRIDGE_NAME + " is already available");
    }

    trace(">ip link set " + DEFAULT_BRIDGE_IF + " up");

    if (const string error = ip_link(ip_fd, DEFAULT_BRIDGE_IF.c_str(), true); !error.empty()) {
        return cleanUp(error);
    }

    trace(">brctl addif " + BRIDGE_NAME + " " + DEFAULT_BRIDGE_IF);

    if (const string error = br_setif(br_socket_fd, BRIDGE_NAME, DEFAULT_BRIDGE_IF, true); !error.empty()) {
        return cleanUp(error);
    }

    close(ip_fd);
    close(br_socket_fd);

    info("Tap device " + DEFAULT_BRIDGE_IF + " created");

    return true;
#endif
}

void CTapDriver::CleanUp() const
{
    if (tap_fd != -1) {
        if (const int br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0); br_socket_fd < 0) {
            LogErrno("Can't open bridge socket");
        } else {
            trace(">brctl delif " + BRIDGE_NAME + " " + DEFAULT_BRIDGE_IF);
            if (const string error = br_setif(br_socket_fd, BRIDGE_NAME, DEFAULT_BRIDGE_IF, false); !error.empty()) {
                warn("Warning: Removing " + DEFAULT_BRIDGE_IF + " from the bridge failed: " + error);
                warn("You may need to manually remove the tap device");
            }
            close(br_socket_fd);
        }

        close(tap_fd);
    }
}

param_map CTapDriver::GetDefaultParams() const
{
    return {
        {   "interface", Join(GetNetworkInterfaces(), ",")},
        {   "inet", DEFAULT_IP}
    };
}

pair<string, string> CTapDriver::ExtractAddressAndMask(const string &s)
{
    string address = s;
    string netmask = DEFAULT_NETMASK;
    if (const auto &components = Split(s, '/', 2); components.size() == 2) {
        address = components[0];

        int m;
        if (!GetAsUnsignedInt(components[1], m) || m < 8 || m > 32) {
            error("Invalid CIDR netmask notation '" + components[1] + "'");
            return {"", ""};
        }

        // long long is required for compatibility with 32 bit platforms
        const auto mask = (long long)(pow(2, 32) - (1 << (32 - m)));
        netmask = to_string((mask >> 24) & 0xff) + '.' + to_string((mask >> 16) & 0xff) + '.' +
            to_string((mask >> 8) & 0xff) + '.' + to_string(mask & 0xff);
    }

    return {address, netmask};
}

string CTapDriver::SetUpEth0(int socket_fd, const string &bridge_interface)
{
#ifdef __linux__
    trace(">brctl addbr " + BRIDGE_NAME);

    if (ioctl(socket_fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) == -1) {
        return "Can't ioctl SIOCBRADDBR";
    }

    trace(">brctl addif " + BRIDGE_NAME + " " + bridge_interface);

    if (const string error = br_setif(socket_fd, BRIDGE_NAME, bridge_interface, true); !error.empty()) {
        return error;
    }

    return "";
#else
    return "SIOCBRADDBR: Linux is required";
#endif
}

string CTapDriver::SetUpNonEth0(int socket_fd, int ip_fd, const string &s)
{
    const auto [address, netmask] = ExtractAddressAndMask(s);
    if (address.empty() || netmask.empty()) {
        return "Error extracting inet address and netmask";
    }

#ifdef __linux__
    trace(">brctl addbr " + BRIDGE_NAME);

    if (ioctl(socket_fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) == -1) {
        return "Can't ioctl SIOCBRADDBR";
    }

    ifreq ifr_a;
    ifr_a.ifr_addr.sa_family = AF_INET;
    strncpy(ifr_a.ifr_name, BRIDGE_NAME.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (auto addr = (sockaddr_in*)&ifr_a.ifr_addr;
    inet_pton(AF_INET, address.c_str(), &addr->sin_addr) != 1) {
        return "Can't convert '" + address + "' into a network address";
    }

    ifreq ifr_n;
    ifr_n.ifr_addr.sa_family = AF_INET;
    strncpy(ifr_n.ifr_name, BRIDGE_NAME.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (auto mask = (sockaddr_in*)&ifr_n.ifr_addr;
    inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr) != 1) {
        return "Can't convert '" + netmask + "' into a netmask";
    }

    trace(">ip address add " + s + " dev " + BRIDGE_NAME);

    if (ioctl(ip_fd, SIOCSIFADDR, &ifr_a) < 0 || ioctl(ip_fd, SIOCSIFNETMASK, &ifr_n) == -1) {
        return "Can't ioctl SIOCSIFADDR or SIOCSIFNETMASK";
    }

    return "";
#else
    return " SIOCSIFADDR/SIOCSIFNETMASK: Linux is required";
#endif
}

string CTapDriver::IpLink(bool enable) const
{
    const int fd = socket(PF_INET, SOCK_DGRAM, 0);
    trace(string(">ip link set " + DEFAULT_BRIDGE_IF + " ") + (enable ? "up" : "down"));
    const string result = ip_link(fd, DEFAULT_BRIDGE_IF.c_str(), enable);
    close(fd);
    return result;
}

void CTapDriver::Flush() const
{
    while (HasPendingPackets()) {
        array<uint8_t, ETH_FRAME_LEN> m_garbage_buffer;
        (void)Receive(m_garbage_buffer.data());
    }
}

bool CTapDriver::HasPendingPackets() const
{
    // Check if there is data that can be received
    pollfd fds;
    fds.fd = tap_fd;
    fds.events = POLLIN | POLLERR;
    fds.revents = 0;
    poll(&fds, 1, 0);
    return fds.revents & POLLIN;
}

// See https://stackoverflow.com/questions/21001659/crc32-algorithm-implementation-in-c-without-a-look-up-table-and-with-a-public-li
uint32_t CTapDriver::Crc32(span<const uint8_t> data)
{
    uint32_t crc = 0xffffffff;
    for (const auto d : data) {
        crc ^= d;
        for (int i = 0; i < 8; i++) {
            const uint32_t mask = -(static_cast<int>(crc) & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

int CTapDriver::Receive(uint8_t *buf) const
{
    // Check if there is data that can be received
    if (!HasPendingPackets()) {
        return 0;
    }

    auto bytes_received = static_cast<uint32_t>(read(tap_fd, buf, ETH_FRAME_LEN));
    if (bytes_received == static_cast<uint32_t>(-1)) {
        warn("Error while receiving a network packet");
        return 0;
    }

    if (bytes_received > 0) {
        // We need to add the Frame Check Status (FCS) CRC back onto the end of the packet.
        // The Linux network subsystem removes it, since most software apps shouldn't ever need it.
        const int crc = Crc32(span(buf, bytes_received));

        buf[bytes_received + 0] = (uint8_t)((crc >> 0) & 0xff);
        buf[bytes_received + 1] = (uint8_t)((crc >> 8) & 0xff);
        buf[bytes_received + 2] = (uint8_t)((crc >> 16) & 0xff);
        buf[bytes_received + 3] = (uint8_t)((crc >> 24) & 0xff);
        bytes_received += 4;
    }

    return bytes_received;
}

int CTapDriver::Send(const uint8_t *buf, int len) const
{
    return static_cast<int>(write(tap_fd, buf, len));
}
