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

    static BusFactory& Instance()
    {
        static BusFactory instance; // NOSONAR instance cannot be inlined
        return instance;
    }

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

    void AddCommand(scsi_command, int, const char*);

    static RpiBus::PiType CheckForPi();

    array<int, 256> command_byte_counts;

    array<string_view, 256> command_names;
};
