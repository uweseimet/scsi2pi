//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "s2pctl_display.h"

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

    bool CommandDevicesInfo();

private:

    bool CommandLogLevel(string_view);
    bool CommandReserveIds(string_view);
    bool CommandCreateImage(string_view);
    bool CommandDeleteImage(string_view);
    bool CommandRenameCopyImage(string_view);
    bool CommandDefaultImageFolder(string_view);
    bool CommandDeviceInfo();
    bool CommandDeviceTypesInfo();
    bool CommandVersionInfo();
    bool CommandServerInfo();
    bool CommandDefaultImageFilesInfo();
    bool CommandImageFileInfo(string_view);
    bool CommandNetworkInterfacesInfo();
    bool CommandLogLevelInfo();
    bool CommandReservedIdsInfo();
    bool CommandMappingInfo();
    bool CommandStatisticsInfo();
    bool CommandOperationInfo();
    bool CommandPropertiesInfo();
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

    [[no_unique_address]] const S2pCtlDisplay s2pctl_display;
};
