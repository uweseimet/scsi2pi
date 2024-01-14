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
#include "base/property_handler.h"

using namespace std;
using namespace filesystem;
using namespace s2p_interface;

class S2pParser
{

public:

    void Banner(span<char*>, bool) const;
    property_map ParseArguments(span<char*>, bool&);

private:

    static vector<char*> ConvertLegacyOptions(const span<char*>&);

    inline static const unordered_map<int, string> OPTIONS_TO_PROPERTIES = {
        { 'p', PropertyHandler::PORT },
        { 'r', PropertyHandler::RESERVED_IDS },
        { 'z', PropertyHandler::LOCALE },
        { 'C', PropertyHandler::PROPERTY_FILE },
        { 'F', PropertyHandler::IMAGE_FOLDER },
        { 'L', PropertyHandler::LOG_LEVEL },
        { 'P', PropertyHandler::TOKEN_FILE },
        { 'R', PropertyHandler::SCAN_DEPTH }
    };
};
