//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include "s2p_util.h"

class parser_exception : public runtime_error
{
    using runtime_error::runtime_error;
};

class io_exception : public runtime_error
{
    using runtime_error::runtime_error;
};

class scsi_exception : public runtime_error
{
    enum sense_key sense_key;
    enum asc asc;

public:

    explicit scsi_exception(enum sense_key sense_key, enum asc asc = asc::no_additional_sense_information)
    : runtime_error(s2p_util::FormatSenseData(sense_key, asc)), sense_key(sense_key), asc(asc)
    {
    }
    ~scsi_exception() override = default;

    enum sense_key get_sense_key() const
    {
        return sense_key;
    }
    enum asc get_asc() const
    {
        return asc;
    }
};
