//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include "rpi_bus.h"

using namespace std;

class BusFactory
{

public:

    using CdbMetaData = struct _CdbMetaData {
        _CdbMetaData(int alo = 0, int als = 0, int bo = 0, int bs = 0)
        : allocation_length_offset(alo), allocation_length_size(als), block_offset(bo), block_size(bs) {}

        int allocation_length_offset;
        int allocation_length_size;
        int block_offset;
        int block_size;
    };

    static BusFactory& Instance()
    {
        static BusFactory instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    CdbMetaData GetCdbMetaData(scsi_command) const;

    unique_ptr<Bus> CreateBus(bool, bool, bool = false);

    int GetCommandBytesCount(scsi_command opcode) const
    {
        return command_byte_counts[static_cast<int>(opcode)];
    }

    auto GetCommandName(scsi_command opcode) const
    {
        return command_names[static_cast<int>(opcode)];
    }

private:

    BusFactory();

    void AddCommand(scsi_command, int, const char*, const CdbMetaData&);

    static RpiBus::PiType CheckForPi();

    // These are arrays instead of maps because of performance reasons
    array<int, 256> command_byte_counts;
    array<string_view, 256> command_names;
    array<CdbMetaData, 256> cdb_meta_data;
};
