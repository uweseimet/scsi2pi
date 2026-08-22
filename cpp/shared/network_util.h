//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <set>
#include <vector>

using namespace std;

struct sockaddr_in;

namespace network_util
{

vector<uint8_t> GetMacAddress(const string&);
set<string, less<>> GetNetworkInterfaces();
optional<sockaddr_in> ResolveHostName(const string&);

}
