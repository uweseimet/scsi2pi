//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
// Interface for SCSI printer commands (see SCSI-2 specification)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiPrinterCommands
{

public:

    ScsiPrinterCommands() = default;
    virtual ~ScsiPrinterCommands() = default;

    // Mandatory commands
    virtual void Print() = 0;

    // ReleaseUnit(), ReserveUnit() and SendDiagnostic() are contributed by PrimaryDevice
};
