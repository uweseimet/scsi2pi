//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <set>
#include "base/device.h"
#include "base/s2p_defs.h"

using namespace filesystem;
using namespace s2p_interface;

class PrimaryDevice;

class CommandResponse
{

public:

    bool GetImageFile(PbImageFile&, const string&) const;
    void GetImageFilesInfo(PbImageFilesInfo&, const string&, const string&) const;
    void GetReservedIds(PbReservedIdsInfo&, const unordered_set<int>&) const;
    void GetDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbServerInfo&) const;
    void GetDevicesInfo(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&) const;
    void GetDeviceTypesInfo(PbDeviceTypesInfo&) const;
    void GetVersionInfo(PbVersionInfo&) const;
    void GetServerInfo(PbServerInfo&, const PbCommand&, const unordered_set<shared_ptr<PrimaryDevice>>&,
        const unordered_set<int>&) const;
    void GetNetworkInterfacesInfo(PbNetworkInterfacesInfo&) const;
    void GetMappingInfo(PbMappingInfo&) const;
    void GetLogLevelInfo(PbLogLevelInfo&) const;
    void GetStatisticsInfo(PbStatisticsInfo&, const unordered_set<shared_ptr<PrimaryDevice>>&) const;
    void GetPropertiesInfo(PbPropertiesInfo&) const;
    void GetOperationInfo(PbOperationInfo&) const;

private:

    void GetDeviceProperties(shared_ptr<PrimaryDevice>, PbDeviceProperties&) const;
    void GetDevice(shared_ptr<PrimaryDevice>, PbDevice&) const;
    void GetAvailableImages(PbImageFilesInfo&, const string&, const string&) const;
    void GetAvailableImages(PbServerInfo&, const string&, const string&) const;
    PbOperationMetaData* CreateOperation(PbOperationInfo&, const PbOperation&, const string&) const;
    void AddOperationParameter(PbOperationMetaData&, const string&, const string&,
        const string& = "", bool = false, const vector<string>& = { }) const;
    set<id_set> MatchDevices(const unordered_set<shared_ptr<PrimaryDevice>>&, PbResult&, const PbCommand&) const;

    static bool ValidateImageFile(const path&);

    static bool FilterMatches(const string&, string_view);

    static bool HasOperation(const set<string, less<>>&, PbOperation);
};
