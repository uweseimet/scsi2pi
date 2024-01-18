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
#include "generated/s2p_interface.pb.h"
#include "shared/s2p_util.h"
#include "base/property_handler.h"

using namespace std;
using namespace s2p_interface;

class S2pParser
{

public:

    void Banner(bool) const;
    property_map ParseArguments(span<char*>, bool&);

private:

    static string ParseBlueScsiFilename(property_map&, const string&, const string&, bool);
    static vector<char*> ConvertLegacyOptions(const span<char*>&);
    static string ParseNumber(const string&);

    inline static const unordered_map<int, string> OPTIONS_TO_PROPERTIES = {
        { 'p', PropertyHandler::PORT },
        { 'r', PropertyHandler::RESERVED_IDS },
        { 'z', PropertyHandler::LOCALE },
        { 'C', PropertyHandler::PROPERTY_FILES },
        { 'F', PropertyHandler::IMAGE_FOLDER },
        { 'L', PropertyHandler::LOG_LEVEL },
        { 'P', PropertyHandler::TOKEN_FILE },
        { 'R', PropertyHandler::SCAN_DEPTH }
    };

    inline static const unordered_map<string, string, s2p_util::StringHash, equal_to<>> BLUE_SCSI_TO_S2P_TYPES = {
        { "CD", "sccd" },
        { "FD", "schd" },
        { "HD", "schd" },
        { "MO", "scmo" },
        { "RE", "scrm" },
        { "TP", "" }
    };
};
