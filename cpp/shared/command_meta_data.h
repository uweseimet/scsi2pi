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

    static CommandMetaData& Instance()
    {
        static CommandMetaData instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    int GetCommandBytesCount(scsi_command opcode) const
    {
        return command_byte_counts[static_cast<int>(opcode)];
    }

    auto GetCommandName(scsi_command opcode) const
    {
        return command_names[static_cast<int>(opcode)];
    }

    string LogCdb(span<const uint8_t>, const string&) const;

private:

    CommandMetaData();

    void AddCommand(scsi_command, int, const char*);

    // These are arrays instead of maps because of performance reasons
    array<int, 256> command_byte_counts;
    array<string_view, 256> command_names;
};
