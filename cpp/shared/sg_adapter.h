//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <spdlog/spdlog.h>
#include "shared/command_meta_data.h"

using namespace spdlog;

class SgAdapter
{

public:

    explicit SgAdapter(logger &l) : sg_logger(l)
    {
    }

    using SgResult = struct {
        int status;
        int length;
    };

    string Init(const string&);
    void CleanUp();

    SgResult SendCommand(span<const uint8_t>, span<uint8_t>, int, int);

    int GetByteCount() const
    {
        return byte_count;
    }

private:

    SgResult SendCommandInternal(span<uint8_t>, span<uint8_t>, int, int, bool);

    bool EvaluateStatus(int, span<uint8_t>, span<uint8_t>);

    void GetBlockSize();

    logger &sg_logger;

    CommandMetaData command_meta_data = CommandMetaData::GetInstance();

    int fd = -1;

    uint32_t block_size = 512;

    int byte_count = 0;

    array<uint8_t, 18> sense_data = { };

    bool sense_data_valid = false;

    // Linux limits the number of bytes that can be transferred in a single SCSI request
    static const int MAX_TRANSFER_LENGTH = 65536;
};
