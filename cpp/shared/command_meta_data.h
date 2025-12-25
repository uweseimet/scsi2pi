//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include "scsi.h"

using namespace std;

class CommandMetaData final
{

public:

    using CdbMetaData = struct {
        int allocation_length_offset;
        int allocation_length_size;
        int block_offset;
        int block_size;
        bool has_data_out;
    };

    static const CommandMetaData& GetInstance()
    {
        static const CommandMetaData instance; // NOSONAR instance cannot be inlined
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

    string LogCdb(span<const uint8_t>, string_view) const;

private:

    CommandMetaData();

    void AddCommand(ScsiCommand, int, string_view, const CdbMetaData&);

    array<int, 256> command_byte_counts;
    array<string, 256> command_names;
    array<CdbMetaData, 256> cdb_meta_data;
};
