//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "base/property_handler.h"
#include <span>

class S2pParser
{

public:

    void Banner(bool) const;
    property_map ParseArguments(span<char*>, bool&) const;

private:

    static string ParseBlueScsiFilename(property_map&, const string&, const string&);
    static vector<char*> ConvertLegacyOptions(const span<char*>&);
    static string ParseNumber(const string&);
};
