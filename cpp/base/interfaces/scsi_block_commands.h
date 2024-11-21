//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
// Interface for SCSI block commands (see https://www.t10.org/drafts.htm, SBC-5)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiBlockCommands
{

public:

    virtual ~ScsiBlockCommands() = default;

    // Mandatory commands
    virtual void FormatUnit() = 0;
    virtual void ReadCapacity10() = 0;
    virtual void ReadCapacity16() = 0;
    virtual void Read10() = 0;
    virtual void Read16() = 0;
    virtual void Write10() = 0;
    virtual void Write16() = 0;

protected:

    ScsiBlockCommands() = default;
};
