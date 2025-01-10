//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include "scsi.h"

using namespace std;

class CommandMetaData
{

public:

    using CdbMetaData = struct {
        int allocation_length_offset;
        int allocation_length_size;
        int block_offset;
        int block_size;
        bool has_data_out;
    };

    static CommandMetaData& Instance()
    {
        static CommandMetaData instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    CdbMetaData GetCdbMetaData(ScsiCommand) const;

    int GetByteCount(ScsiCommand cmd) const
    {
        return command_byte_counts[static_cast<int>(cmd)];
    }

    const string& GetCommandName(ScsiCommand cmd) const
    {
        return command_names[static_cast<int>(cmd)];
    }

    string LogCdb(span<const uint8_t>, const string&) const;

private:

    CommandMetaData();

    void AddCommand(ScsiCommand, int, string_view, const CdbMetaData&);

    // These are arrays instead of maps because of performance reasons
    array<int, 256> command_byte_counts;
    array<string, 256> command_names;
    array<CdbMetaData, 256> cdb_meta_data;
};
