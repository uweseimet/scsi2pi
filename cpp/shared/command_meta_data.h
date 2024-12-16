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

    using CdbMetaData = struct _CdbMetaData {
        _CdbMetaData(int alo = 0, int als = 0, int bo = 0, int bs = 0, bool d = false)
        : allocation_length_offset(alo), allocation_length_size(als), block_offset(bo), block_size(bs), has_data_out(d) {}

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

    CdbMetaData GetCdbMetaData(scsi_command) const;

    int GetByteCount(scsi_command cmd) const
    {
        return command_byte_counts[static_cast<int>(cmd)];
    }

    auto GetCommandName(scsi_command cmd) const
    {
        return command_names[static_cast<int>(cmd)];
    }

    string LogCdb(span<const uint8_t>) const;

private:

    CommandMetaData();

    void AddCommand(scsi_command, int, string_view, const CdbMetaData&);

    // These are arrays instead of maps because of performance reasons
    array<int, 256> command_byte_counts;
    array<string, 256> command_names;
    array<CdbMetaData, 256> cdb_meta_data;
};
