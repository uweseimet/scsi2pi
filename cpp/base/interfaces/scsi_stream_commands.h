//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Interface for SCSI streamer commands (see SCSI-2 and SSC-2 specification)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiStreamCommands
{

public:

    virtual ~ScsiStreamCommands() = default;

    // Mandatory commands
    virtual void Erase6() = 0;
    virtual void Read6() = 0;
    virtual void ReadBlockLimits() = 0;
    virtual void Rewind() = 0;
    virtual void Space6() = 0;
    virtual void Write6() = 0;
    virtual void WriteFilemarks6() = 0;

protected:

    ScsiStreamCommands() = default;
};
