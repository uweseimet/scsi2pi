//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include "s2p_util.h"

class ParserException final : public runtime_error
{
    using runtime_error::runtime_error;
};

class IoException final : public runtime_error
{
    using runtime_error::runtime_error;
};

class ScsiException final : public runtime_error
{
    SenseKey sense_key;
    Asc asc;

public:

    explicit ScsiException(SenseKey s, Asc a = Asc::NO_ADDITIONAL_SENSE_INFORMATION)
    : runtime_error(s2p_util::FormatSenseData(s, a)), sense_key(s), asc(a)
    {
    }
    ~ScsiException() override = default;

    SenseKey GetSenseKey() const
    {
        return sense_key;
    }
    Asc GetAsc() const
    {
        return asc;
    }
};
