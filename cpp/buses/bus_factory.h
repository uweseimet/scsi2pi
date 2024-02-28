//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "bus.h"

using namespace std;

class BusFactory
{

public:

    static BusFactory& Instance()
    {
        static BusFactory instance; // NOSONAR instance cannot be inlined
        return instance;
    }

    unique_ptr<Bus> CreateBus(bool, bool);

    int GetCommandBytesCount(scsi_command opcode) const
    {
        return command_byte_counts[static_cast<int>(opcode)];
    }

    string GetCommandName(scsi_command opcode) const
    {
        return command_names[static_cast<int>(opcode)];
    }

    bool IsRaspberryPi() const
    {
        return is_raspberry_pi;
    }

private:

    BusFactory();

    void AddCommand(scsi_command, int, string_view);

    bool CheckForPi();

    bool is_raspberry_pi = false;

    array<int, 256> command_byte_counts;

    array<string, 256> command_names;
};
