//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class S2pCtlDisplay
{

public:

    S2pCtlDisplay() = default;
    ~S2pCtlDisplay() = default;

    string DisplayDevicesInfo(const PbDevicesInfo&) const;
    string DisplayDeviceInfo(const PbDevice&) const;
    string DisplayVersionInfo(const PbVersionInfo&) const;
    string DisplayLogLevelInfo(const PbLogLevelInfo&) const;
    string DisplayDeviceTypesInfo(const PbDeviceTypesInfo&) const;
    string DisplayReservedIdsInfo(const PbReservedIdsInfo&) const;
    string DisplayImageFile(const PbImageFile&) const;
    string DisplayImageFilesInfo(const PbImageFilesInfo&) const;
    string DisplayNetworkInterfaces(const PbNetworkInterfacesInfo&) const;
    string DisplayMappingInfo(const PbMappingInfo&) const;
    string DisplayStatisticsInfo(const PbStatisticsInfo&) const;
    string DisplayOperationInfo(const PbOperationInfo&) const;

private:

    string DisplayParams(const PbDevice&) const;
    string DisplayAttributes(const PbDeviceProperties&) const;
    string DisplayDefaultParameters(const PbDeviceProperties&) const;
    string DisplayBlockSizes(const PbDeviceProperties&) const;
    string DisplayParameters(const PbOperationMetaData&) const;
    string DisplayPermittedValues(const PbOperationParameter&) const;
};
