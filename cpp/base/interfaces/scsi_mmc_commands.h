//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
// Interface for SCSI Multi-Media commands (see https://www.t10.org/drafts.htm, MMC-6)
//
//---------------------------------------------------------------------------

#pragma once

class ScsiMmcCommands
{

public:

    virtual ~ScsiMmcCommands() = default;

    virtual void ReadToc() = 0;

protected:

    ScsiMmcCommands() = default;
};
