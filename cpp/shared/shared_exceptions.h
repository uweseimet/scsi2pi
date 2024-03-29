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

using namespace std;

class parser_exception : public runtime_error
{
    using runtime_error::runtime_error;
};

class io_exception : public runtime_error
{
    using runtime_error::runtime_error;
};

class file_not_found_exception : public io_exception
{
    using io_exception::io_exception;
};

class scsi_exception : public exception
{
    scsi_defs::sense_key sense_key;
    scsi_defs::asc asc;

    string message;

public:

    explicit scsi_exception(scsi_defs::sense_key sense_key,
        scsi_defs::asc asc = scsi_defs::asc::no_additional_sense_information)
    : sense_key(sense_key), asc(asc), message(s2p_util::FormatSenseData(sense_key, asc))
    {
    }
    ~scsi_exception() override = default;

    scsi_defs::sense_key get_sense_key() const
    {
        return sense_key;
    }
    scsi_defs::asc get_asc() const
    {
        return asc;
    }

    const char* what() const noexcept override
    {
        return message.c_str();
    }
};
