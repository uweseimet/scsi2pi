//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
// Interface for SCSI primary commands (see https://www.t10.org/drafts.htm, SPC-6)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiPrimaryCommands
{

public:

    virtual ~ScsiPrimaryCommands() = default;

    // Mandatory commands
    virtual void TestUnitReady() = 0;
    virtual void Inquiry() = 0;
    virtual void ReportLuns() = 0;

    // Optional commands implemented by all device types
    virtual void RequestSense() = 0;
    virtual void ReleaseUnit() = 0;
    virtual void ReserveUnit() = 0;
    virtual void SendDiagnostic() = 0;

protected:

    ScsiPrimaryCommands() = default;
};
