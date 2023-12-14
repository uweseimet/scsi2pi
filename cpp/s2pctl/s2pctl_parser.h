//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <string>
#include <unordered_map>
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class S2pCtlParser
{

public:

    S2pCtlParser() = default;
    ~S2pCtlParser() = default;

    PbOperation ParseOperation(string_view) const;
    PbDeviceType ParseType(const string&) const;

private:

    const unordered_map<int, PbOperation> operations = {
        { 'a', ATTACH },
        { 'd', DETACH },
        { 'e', EJECT },
        { 'i', INSERT },
        { 'p', PROTECT },
        { 's', DEVICES_INFO },
        { 'u', UNPROTECT }
    };

    const unordered_map<int, PbDeviceType> device_types = {
        { 'c', SCCD },
        { 'd', SCDP },
        { 'h', SCHD },
        { 'l', SCLP },
        { 'm', SCMO },
        { 'r', SCRM },
        { 's', SCHS }
    };
};
