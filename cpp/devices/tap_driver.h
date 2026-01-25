//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
// for Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "base/device.h"
#include "shared/s2p_defs.h"

#ifndef ETH_FRAME_LEN
static constexpr int ETH_FRAME_LEN = 1514;
#endif

class TapDriver final
{

public:

    TapDriver();
    ~TapDriver() = default;

    string Init(const param_map&, logger&);
    void CleanUp(logger&) const;

    param_map GetDefaultParams() const;

    int Receive(data_in_t, logger&) const;
    int Send(data_out_t) const;
    bool HasPendingPackets() const;

    void Flush(logger&) const;

    static uint32_t Crc32(span<const uint8_t>);

    static const string& GetBridgeName()
    {
        return BRIDGE_NAME;
    }

    // Enable/Disable the piscsi0 interface
    static string IpLink(bool, logger&);

private:

    string AddBridge(int, logger&) const;
    string DeleteBridge(int, logger&) const;

    static string IpLink(int, const string&, bool);
    static string BrSetIf(int fd, const string&, bool);
    string CreateBridge(int, int, logger&);
    pair<string, string> ExtractAddressAndMask(logger&) const;
    string SetAddressAndNetMask(int, const string&, logger&) const;

    int tap_fd = -1;

    set<string, less<>> available_interfaces;

    string inet;

    string bridge_interface;

    bool bridge_created = false;

    inline static const string BRIDGE_INTERFACE_NAME = "piscsi0";
    inline static const string BRIDGE_NAME = "piscsi_bridge";

    static constexpr const char *DEFAULT_IP = "10.10.20.1/24"; // NOSONAR This hardcoded IP address is safe
    static constexpr const char *DEFAULT_NETMASK = "255.255.255.0"; // NOSONAR This hardcoded netmask is safe

    static constexpr const char *BRIDGE = "bridge";
    static constexpr const char *INET = "inet";
    static constexpr const char *INTERFACE = "interface";
};

