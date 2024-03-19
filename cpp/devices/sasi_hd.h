//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_set>
#include "disk.h"

class SasiHd : public Disk
{

public:

    explicit SasiHd(int, const unordered_set<uint32_t>& = { 256, 512, 1024 });
    ~SasiHd() override = default;

    void Open() override;

    void Inquiry() override;
    vector<uint8_t> InquiryInternal() const override;
    void RequestSense() override;
};
