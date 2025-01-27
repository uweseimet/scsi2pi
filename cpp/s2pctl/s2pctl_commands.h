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

struct sockaddr_in;

class S2pCtlCommands
{

public:

    S2pCtlCommands(PbCommand &command, const string &hostname, int port, const string &b, const string &j,
        const string &t)
    : command(command), hostname(hostname), port(port), filename_binary(b), filename_json(j), filename_text(t)
    {
    }
    ~S2pCtlCommands() = default;

    bool Execute(string_view, string_view, string_view, string_view, string_view);

    bool HandleDevicesInfo();

private:

    bool HandleLogLevel(string_view);
    bool HandleReserveIds(string_view);
    bool HandleCreateImage(string_view);
    bool HandleDeleteImage(string_view);
    bool HandleRenameCopyImage(string_view);
    bool HandleDefaultImageFolder(string_view);
    bool HandleDeviceInfo();
    bool HandleDeviceTypesInfo();
    bool HandleVersionInfo();
    bool HandleServerInfo();
    bool HandleDefaultImageFilesInfo();
    bool HandleImageFileInfo(string_view);
    bool HandleNetworkInterfacesInfo();
    bool HandleLogLevelInfo();
    bool HandleReservedIdsInfo();
    bool HandleMappingInfo();
    bool HandleStatisticsInfo();
    bool HandleOperationInfo();
    bool HandlePropertiesInfo();
    bool SendCommand();
    bool EvaluateParams(string_view, const string&, const string&);

    void ExportAsBinary(const PbCommand&, const string&) const;
    void ExportAsJson(const PbCommand&, const string&) const;
    void ExportAsText(const PbCommand&, const string&) const;

    PbCommand &command;
    string hostname;
    int port;

    string filename_binary;
    string filename_json;
    string filename_text;

    PbResult result;
};
