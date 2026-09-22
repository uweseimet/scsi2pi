//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "shared/runnable.h"
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class S2pCtl final : public Runnable
{

public:

    int Run(span<char*>) override;

private:

    void Banner(bool) const;
    int RunInteractive();
    int ParseArguments(const span<char*>);

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
