//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
// for Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include "base/device.h"

#ifndef ETH_FRAME_LEN
static constexpr int ETH_FRAME_LEN = 1514;
#endif
#ifndef ETH_FCS_LEN
static constexpr int ETH_FCS_LEN = 4;
#endif

using namespace std;

class TapDriver
{

public:

    TapDriver();
    ~TapDriver() = default;

    bool Init(const param_map&);
    void CleanUp() const;

    param_map GetDefaultParams() const;

    int Receive(uint8_t*) const;
    int Send(const uint8_t*, int) const;
    bool HasPendingPackets() const;

    void Flush() const;

    static uint32_t Crc32(span<const uint8_t>);

    static string GetBridgeName()
    {
        return BRIDGE_NAME;
    }

    string AddBridge(int) const;
    string DeleteBridge(int) const;

    // Enable/Disable the piscsi0 interface
    static string IpLink(bool);

private:

    static string IpLink(int, const string&, bool);
    static string BrSetif(int fd, const string&, const string&, bool);
    string CreateBridge(int, int);
    static pair<string, string> ExtractAddressAndMask(const string&);
    string SetAddressAndNetMask(int, const string&) const;

    int tap_fd = -1;

    set<string, less<>> available_interfaces;

    string inet;

    string bridge_interface;

    bool create_bridge = true;

    bool bridge_created = false;

    const inline static string BRIDGE_INTERFACE_NAME = "piscsi0";
    const inline static string BRIDGE_NAME = "piscsi_bridge";

    const inline static string DEFAULT_IP = "10.10.20.1/24"; // NOSONAR This hardcoded IP address is safe
    const inline static string DEFAULT_NETMASK = "255.255.255.0"; // NOSONAR This hardcoded netmask is safe
};

