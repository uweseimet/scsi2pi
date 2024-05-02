//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Interface for SCSI communications devices commands (see SCSI-2 specification)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiCommunicationsCommands
{

public:

    virtual ~ScsiCommunicationsCommands() = default;

    // Mandatory commands
    virtual void SendMessage6() const = 0;

    // Optional commands
    virtual void GetMessage6() = 0;

protected:

    ScsiCommunicationsCommands() = default;
};
