//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <set>
#include "initiator/initiator_executor.h"

using namespace std;

class S2pDumpExecutor
{

public:

    S2pDumpExecutor(Bus &bus, int id, logger &l) : initiator_executor(make_unique<InitiatorExecutor>(bus, id, l))
    {
    }

    void TestUnitReady() const;
    void RequestSense() const;
    bool Inquiry(span<uint8_t>);
    bool ModeSense6(span<uint8_t>);
    set<int> ReportLuns();

    void SetTarget(int id, int lun, bool sasi)
    {
        initiator_executor->SetTarget(id, lun, sasi);
    }

protected:

    logger& GetLogger()
    {
        return initiator_executor->GetLogger();
    }

    unique_ptr<InitiatorExecutor> initiator_executor;
};
