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
#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif

using namespace std;

namespace network_util
{

vector<uint8_t> GetMacAddress(const string&);
set<string, less<>> GetNetworkInterfaces();
#if __has_include(<netinet/in.h>)
optional<sockaddr_in> ResolveHostName(const string&);
#endif

}
