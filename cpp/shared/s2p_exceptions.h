//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <stdexcept>
#include <spdlog/spdlog.h>
#include "s2p_util.h"

class ParserException final : public runtime_error
{
public:

    using runtime_error::runtime_error;

    template<typename ... Args>
    explicit ParserException(fmt::format_string<Args...> fmt, Args &&... args)
    : runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {
    }
};

class IoException final : public runtime_error
{
public:

    using runtime_error::runtime_error;

    template<typename ... Args>
    explicit IoException(fmt::format_string<Args...> fmt, Args &&... args)
    : runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {
    }
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
