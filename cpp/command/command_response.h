//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_set>
#include <spdlog/spdlog.h>
#include "generated/target_api.pb.h"

class PrimaryDevice;

using namespace std;
using namespace spdlog;
using namespace s2p_interface;

namespace command_response
{

bool GetImageFile(PbImageFile&, const string&);
void GetImageFilesInfo(PbImageFilesInfo&, const string&, const string&, logger&);
void GetReservedIds(PbReservedIdsInfo&, const unordered_set<int>&);
void GetDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbServerInfo&);
void GetDevicesInfo(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&);
void GetDeviceTypesInfo(PbDeviceTypesInfo&);
void GetVersionInfo(PbVersionInfo&);
void GetServerInfo(PbServerInfo&, const PbCommand&, const unordered_set<shared_ptr<PrimaryDevice>>&,
    const unordered_set<int>&, logger&);
void GetNetworkInterfacesInfo(PbNetworkInterfacesInfo&);
void GetMappingInfo(PbMappingInfo&);
void GetLogLevelInfo(PbLogLevelInfo&);
void GetStatisticsInfo(PbStatisticsInfo&, const unordered_set<shared_ptr<PrimaryDevice>>&);
void GetPropertiesInfo(PbPropertiesInfo&);
void GetOperationInfo(PbOperationInfo&);

}
