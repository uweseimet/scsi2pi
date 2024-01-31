//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_parser.h"

PbOperation S2pCtlParser::ParseOperation(string_view operation) const
{
    const auto &it = operations.find(tolower(operation[0]));
    return it != operations.end() ? it->second : NO_OPERATION;
}

PbDeviceType S2pCtlParser::ParseType(const string &type) const
{
    string t;
    ranges::transform(type, back_inserter(t), ::toupper);

    if (PbDeviceType parsed_type; PbDeviceType_Parse(t, &parsed_type)) {
        return parsed_type;
    }

    // Handle convenience device types (shortcuts)
    const auto &it = device_types.find(tolower(type[0]));
    return it != device_types.end() ? it->second : UNDEFINED;
}
