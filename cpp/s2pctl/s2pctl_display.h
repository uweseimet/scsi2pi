//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/target_api.pb.h"

using namespace std;
using namespace s2p_interface;

namespace s2pctl_display
{

string DisplayDevicesInfo(const PbDevicesInfo&);
string DisplayDeviceInfo(const PbDevice&);
string DisplayVersionInfo(const PbVersionInfo&);
string DisplayLogLevelInfo(const PbLogLevelInfo&);
string DisplayDeviceTypesInfo(const PbDeviceTypesInfo&);
string DisplayReservedIdsInfo(const PbReservedIdsInfo&);
string DisplayImageFile(const PbImageFile&);
string DisplayImageFilesInfo(const PbImageFilesInfo&);
string DisplayNetworkInterfaces(const PbNetworkInterfacesInfo&);
string DisplayMappingInfo(const PbMappingInfo&);
string DisplayStatisticsInfo(const PbStatisticsInfo&);
string DisplayOperationInfo(const PbOperationInfo&);
string DisplayPropertiesInfo(const PbPropertiesInfo&);

}
