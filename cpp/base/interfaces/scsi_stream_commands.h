//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
// Interface for SCSI streamer commands (see SCSI-2 specification)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiStreamCommands
{

public:

    virtual ~ScsiStreamCommands() = default;

    // Mandatory commands
    virtual void Erase() = 0;
    virtual void Read() = 0;
    virtual void ReadBlockLimits() = 0;
    virtual void Rewind() = 0;
    virtual void Space() = 0;
    virtual void Write() = 0;
    virtual void WriteFilemarks() = 0;

protected:

    ScsiStreamCommands() = default;
};
