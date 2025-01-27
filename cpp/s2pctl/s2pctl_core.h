//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "generated/target_api.pb.h"

using namespace std;
using namespace s2p_interface;

class S2pCtl
{

public:

    int Run(const vector<char*>&);

private:

    void Banner(bool) const;
    int RunInteractive();
    int ParseArguments(const vector<char*>&);

    static PbOperation ParseOperation(string_view);

    // Preserve host settings during invocations in interactive mode
    string hostname = "localhost";
    int port = 6868;

    inline static const unordered_map<int, PbOperation> OPERATIONS = {
        { 'a', ATTACH },
        { 'd', DETACH },
        { 'e', EJECT },
        { 'i', INSERT },
        { 'p', PROTECT },
        { 's', DEVICES_INFO },
        { 'u', UNPROTECT }
    };
};
