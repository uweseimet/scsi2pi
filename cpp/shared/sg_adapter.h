//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <spdlog/spdlog.h>
#include "shared/scsi.h"

using namespace std;
using namespace spdlog;

class SgAdapter
{

public:

    explicit SgAdapter(logger &l) : sg_logger(l)
    {
    }

    using SgResult = struct _SgResult {
        int status = 0;
        int length = 0;
    };

    string Init(const string&);
    void CleanUp();

    SgResult SendCommand(span<uint8_t>, span<uint8_t>, int, int);

private:

    logger &sg_logger;

    int fd = -1;

    array<uint8_t, 18> sense_data = { };

    bool sense_data_valid = false;
};
