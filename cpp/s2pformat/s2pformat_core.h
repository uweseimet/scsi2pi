//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include "shared/sg_adapter.h"

using namespace std;

class S2pFormat
{

public:

    int Run(span<char*>);

private:

    using FormatDescriptor = struct _FormatDescriptor {
        _FormatDescriptor(uint32_t b, uint32_t l) : blocks(b), length(l) {}

        uint32_t blocks;
        uint32_t length;
    };

    void Banner(bool) const;
    bool ParseArguments(span<char*>);

    vector<FormatDescriptor> GetFormatDescriptors();
    int SelectFormat(span<const S2pFormat::FormatDescriptor>);
    string Format(span<const S2pFormat::FormatDescriptor>, int);
    int ExecuteCommand(span<uint8_t>, span<uint8_t>, int);

    unique_ptr<SgAdapter> sg_adapter;

    string device;
};
