//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <vector>
#include "base/property_handler.h"

using namespace std;

class S2pParser
{

public:

    S2pParser() = default;
    ~S2pParser() = default;

    void Banner(bool) const;
    property_map ParseArguments(span<char*>, bool&) const;

private:

    static string ParseBlueScsiFilename(property_map&, const string&, const string&, bool);
    static vector<char*> ConvertLegacyOptions(const span<char*>&);
    static string ParseNumber(const string&);
};
