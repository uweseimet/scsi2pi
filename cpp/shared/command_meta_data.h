//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
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
        bool has_custom_data_out;
    };

    CommandMetaData(const CommandMetaData&) = delete;
    CommandMetaData& operator=(const CommandMetaData&) = delete;

    static const CommandMetaData& GetInstance()
    {
        static const CommandMetaData instance;
        return instance;
    }

    const CdbMetaData& GetCdbMetaData(ScsiCommand cmd) const
    {
        return cdb_meta_data[to_underlying(cmd)];
    }

    int GetByteCount(ScsiCommand cmd) const
    {
        return command_byte_counts[to_underlying(cmd)];
    }

    const string& GetCommandName(ScsiCommand cmd) const
    {
        return command_names[to_underlying(cmd)];
    }

    string LogCdb(span<const uint8_t>, string_view) const;

private:

    CommandMetaData();

    void AddCommand(ScsiCommand, int, string, const CdbMetaData&);

    array<int, 256> command_byte_counts = { };
    array<string, 256> command_names = { };
    array<CdbMetaData, 256> cdb_meta_data = { };
};
