//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) akuker
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <span>
#include "base/device.h"

#ifndef ETH_FRAME_LEN
static const int ETH_FRAME_LEN = 1514;
#endif
#ifndef ETH_FCS_LEN
static const int ETH_FCS_LEN = 4;
#endif

using namespace std;

class CTapDriver
{
    static const string BRIDGE_NAME;

    const inline static string DEFAULT_IP = "10.10.20.1/24"; // NOSONAR This hardcoded IP address is safe

public:

    CTapDriver() = default;
    ~CTapDriver() = default;
    CTapDriver(CTapDriver&) = default;
    CTapDriver& operator=(const CTapDriver&) = default;

    bool Init(const param_map&);
    void CleanUp() const;

    param_map GetDefaultParams() const;

    int Receive(uint8_t*) const;
    int Send(const uint8_t*, int) const;
    bool HasPendingPackets() const;
    // Enable/Disable the piscsi0 interface
    string IpLink(bool) const;
    // Purge all of the packets that are waiting to be processed
    void Flush() const;

    static uint32_t Crc32(span<const uint8_t>);

    static string GetBridgeName()
    {
        return BRIDGE_NAME;
    }

private:

    static string SetUpEth0(int, const string&);
    static string SetUpNonEth0(int, int, const string&);
    static pair<string, string> ExtractAddressAndMask(const string&);

    // File handle
    int m_hTAP = -1;

    // Prioritized comma-separated list of interfaces to create the bridge for
    vector<string> interfaces;

    string inet;
};

