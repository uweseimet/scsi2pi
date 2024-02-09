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
#include "tap_driver.h"

using namespace spdlog;
using namespace s2p_util;
using namespace network_util;

TapDriver::TapDriver()
{
    available_interfaces = GetNetworkInterfaces();
}

bool TapDriver::Init(const param_map &const_params)
{
    param_map params = const_params;
    stringstream s(params["interface"]);
    string interface;
    while (getline(s, interface, ',')) {
        if (available_interfaces.contains(interface)) {
            bridge_interface = interface;
            break;
        }
    }

    if (bridge_interface.empty()) {
        error("No valid network interfaces available");
        return false;
    }

    if ((tap_fd = open("/dev/net/tun", O_RDWR)) == -1) {
        error("Can't open /dev/net/tun: {}", strerror(errno));
        return false;
    }

#ifndef __linux__
    return false;
#else

    inet = params["inet"];

    trace("Setting up TAP interface " + BRIDGE_INTERFACE_NAME);

    // IFF_NO_PI for no extra packet information
    ifreq ifr = { };
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, BRIDGE_INTERFACE_NAME.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (const int ret = ioctl(tap_fd, TUNSETIFF, (void*)&ifr); ret == -1) {
        error("Can't ioctl TUNSETIFF: {}", strerror(errno));
        close(tap_fd);
        return false;
    }

    const int ip_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (ip_fd == -1) {
        error("Can't open IP socket: {}", strerror(errno));
        close(tap_fd);
        return false;
    }

    const auto &cleanUp = [this, ip_fd](const string &msg) {
        error(msg);
        close(ip_fd);
        close(tap_fd);
        return false;
    };

    if (const string error = IpLink(true); !error.empty()) {
        return cleanUp(error);
    }

    // Only physical interfaces need a bridge
    if (bridge_interface.starts_with("eth")) {
        const int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (fd == -1) {
            return cleanUp(fmt::format("Can't open bridge socket: {}", strerror(errno)));
        }

        if (const string error = CreateBridge(fd, ip_fd); !error.empty()) {
            close(fd);
            return cleanUp(error);
        }

        trace(">brctl addif " + BRIDGE_NAME + " " + BRIDGE_INTERFACE_NAME);
        const string error = BrSetif(fd, BRIDGE_NAME, BRIDGE_INTERFACE_NAME, true);
        close(fd);
        if (!error.empty()) {
            return cleanUp(error);
        }
    }
    else {
        trace(">ip addr add {0} brd + dev {1}", inet, BRIDGE_INTERFACE_NAME);
        if (const string error = SetAddressAndNetMask(ip_fd, BRIDGE_INTERFACE_NAME); !error.empty()) {
            return cleanUp(error);
        }
    }

    close(ip_fd);

    info("TAP interface " + BRIDGE_INTERFACE_NAME + " created");

    return true;
#endif
}

void TapDriver::CleanUp() const
{
    if (tap_fd == -1) {
        return;
    }

    if (bridge_created) {
        if (const int fd = socket(AF_LOCAL, SOCK_STREAM, 0); fd == -1) {
            error("Can't open bridge socket: {}", strerror(errno));
        } else {
            trace(">brctl delif " + BRIDGE_NAME + " " + BRIDGE_INTERFACE_NAME);
            if (const string error = BrSetif(fd, BRIDGE_NAME, BRIDGE_INTERFACE_NAME, false); !error.empty()) {
                warn("Removing {0} from {1} failed: {2}", BRIDGE_INTERFACE_NAME, BRIDGE_NAME, error);
                warn("You may need to manually remove the TAP device");
            }

            trace(">ip link set dev " + BRIDGE_NAME + " down");
            if (const string error = IpLink(fd, BRIDGE_NAME, false); !error.empty()) {
                warn(error);
            }

            if (const string error = DeleteBridge(fd); !error.empty()) {
                warn(error);
            }

            close(fd);
        }
    }

    close(tap_fd);
}

param_map TapDriver::GetDefaultParams() const
{
    return {
        {   "interface", Join(available_interfaces, ",")},
        {   "inet", DEFAULT_IP}
    };
}

string TapDriver::CreateBridge(int bridge_fd, int ip_fd)
{
    // Check if the bridge has already been created manually by checking whether there is a MAC address for it
    if (GetMacAddress(BRIDGE_NAME).empty()) {
        info("Creating {0} for interface {1}", BRIDGE_NAME, bridge_interface);

        if (const string &error = AddBridge(bridge_fd); !error.empty()) {
            return error;
        }

        trace(">ip link set dev " + BRIDGE_NAME + " up");
        if (const string error = IpLink(ip_fd, BRIDGE_NAME, true); !error.empty()) {
            return error;
        }

        bridge_created = true;
    }

    return "";
}

string TapDriver::SetAddressAndNetMask(int fd, const string &interface) const
{
    const auto [address, netmask] = ExtractAddressAndMask(inet);
    if (address.empty() || netmask.empty()) {
        return "Error extracting inet address and netmask";
    }

#ifdef __linux__
    ifreq ifr_a;
    ifr_a.ifr_addr.sa_family = AF_INET;
    strncpy(ifr_a.ifr_name, interface.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (auto addr = (sockaddr_in*)&ifr_a.ifr_addr;
    inet_pton(AF_INET, address.c_str(), &addr->sin_addr) != 1) {
        return "Can't convert '" + address + "' into a network address";
    }

    ifreq ifr_n;
    ifr_n.ifr_addr.sa_family = AF_INET;
    strncpy(ifr_n.ifr_name, interface.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (auto mask = (sockaddr_in*)&ifr_n.ifr_addr;
    inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr) != 1) {
        return "Can't convert '" + netmask + "' into a netmask";
    }

    if (ioctl(fd, SIOCSIFADDR, &ifr_a) == -1 || ioctl(fd, SIOCSIFNETMASK, &ifr_n) == -1) {
        return "Can't ioctl SIOCSIFADDR or SIOCSIFNETMASK";
    }
#endif

    return "";
}

pair<string, string> TapDriver::ExtractAddressAndMask(const string &addr)
{
    string address = addr;
    string netmask = DEFAULT_NETMASK;
    if (const auto &components = Split(addr, '/', 2); components.size() == 2) {
        address = components[0];

        int m;
        if (!GetAsUnsignedInt(components[1], m) || m < 8 || m > 32) {
            error("Invalid CIDR netmask notation '{}'", components[1]);
            return {"", ""};
        }

        // long long is required for compatibility with 32 bit platforms
        const auto mask = (long long)(pow(2, 32) - (1 << (32 - m)));
        netmask = to_string((mask >> 24) & 0xff) + '.' + to_string((mask >> 16) & 0xff) + '.' +
            to_string((mask >> 8) & 0xff) + '.' + to_string(mask & 0xff);
    }

    return {address, netmask};
}

string TapDriver::AddBridge(int fd) const
{
#ifdef __linux__
    trace(">brctl addbr " + BRIDGE_NAME);
    if (ioctl(fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) == -1) {
        return "Can't ioctl SIOCBRADDBR";
    }

    trace(">brctl addif {0} {1}", BRIDGE_NAME, bridge_interface);
    if (const string error = BrSetif(fd, BRIDGE_NAME, bridge_interface, true); !error.empty()) {
        return error;
    }
#endif

    return "";
}

string TapDriver::DeleteBridge(int fd) const
{
#ifdef __linux__
    if (bridge_created) {
        trace(">brctl delbr " + BRIDGE_NAME);
        if (ioctl(fd, SIOCBRDELBR, BRIDGE_NAME.c_str()) == -1) {
            return "Removing " + BRIDGE_NAME + " failed: " + strerror(errno);
        }
    }
#endif

    return "";
}

string TapDriver::IpLink(bool up)
{
    const int fd = socket(PF_INET, SOCK_DGRAM, 0);
    trace(string(">ip link set " + BRIDGE_INTERFACE_NAME + " ") + (up ? "up" : "down"));
    const string result = IpLink(fd, BRIDGE_INTERFACE_NAME, up);
    close(fd);

    return result;
}

string TapDriver::IpLink(int fd, const string &interface, bool up)
{
    ifreq ifr;
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
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

string TapDriver::BrSetif(int fd, const string &bridge, const string &interface, bool add)
{
#ifdef __linux__
    ifreq ifr;
    ifr.ifr_ifindex = if_nametoindex(interface.c_str());
    if (!ifr.ifr_ifindex) {
        return "Can't if_nametoindex " + interface;
    }

    strncpy(ifr.ifr_name, bridge.c_str(), IFNAMSIZ - 1); // NOSONAR Using strncpy is safe
    if (ioctl(fd, add ? SIOCBRADDIF : SIOCBRDELIF, &ifr) == -1) {
        return "Can't ioctl " + string(add ? "SIOCBRADDIF" : "SIOCBRDELIF");
    }
#endif

    return "";
}

void TapDriver::Flush() const
{
    while (HasPendingPackets()) {
        array<uint8_t, ETH_FRAME_LEN> m_garbage_buffer;
        (void)Receive(m_garbage_buffer.data());
    }
}

bool TapDriver::HasPendingPackets() const
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
uint32_t TapDriver::Crc32(span<const uint8_t> data)
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

int TapDriver::Receive(uint8_t *buf) const
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

int TapDriver::Send(const uint8_t *buf, int len) const
{
    return static_cast<int>(write(tap_fd, buf, len));
}
