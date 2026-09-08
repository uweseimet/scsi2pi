//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>

class Runnable
{

public:

    virtual ~Runnable() = default;

    virtual int Run(std::span<char*>) = 0;
};
