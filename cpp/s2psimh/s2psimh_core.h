//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>
#include "shared/s2p_formatter.h"
#include "shared/simh_util.h"

class S2pSimh final
{

public:

    int Run(span<char*>);

private:

    static void Banner(bool);

    bool ParseArguments(span<char*>);

    int Add();
    int Analyze();

    void PrintClass(const simh_util::SimhMetaData&) const;
    void PrintValue(const simh_util::SimhMetaData&);
    bool PrintRecord(const string&, const simh_util::SimhMetaData&);
    bool PrintReservedMarker(const simh_util::SimhMetaData&);

    bool ReadRecord(span<uint8_t>);

    vector<simh_util::SimhMetaData> ParseObject(const string&);

    S2pFormatter formatter;

    string simh_filename;

    string binary_data_filename;
    string hex_data_filename;

    fstream simh_file;
    ifstream data_file;

    off_t simh_file_size;

    int64_t position = 0;
    int64_t old_position = 0;

    bool dump = false;

    vector<simh_util::SimhMetaData> meta_data;

    uint32_t limit = numeric_limits<uint32_t>::max();
};
