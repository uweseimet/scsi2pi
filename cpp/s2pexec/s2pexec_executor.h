//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "initiator/initiator_executor.h"
#include "shared/sg_adapter.h"

using namespace std;

class S2pExecExecutor
{

public:

    explicit S2pExecExecutor(logger &logger) : s2pexec_logger(logger)
    {
    }

    string Init(const string&);
    string Init(int, const string&, bool);
    void CleanUp();

    bool IsSg() const
    {
        return is_sg;
    }

    void ResetBus();

    int ExecuteCommand(vector<uint8_t>&, vector<uint8_t>&, int, bool);

    tuple<SenseKey, Asc, int> GetSenseData() const;

    int GetByteCount() const;

    void SetTarget(int, int, bool);

    void SetLimit(int limit)
    {
        if (initiator_executor) {
            initiator_executor->SetLimit(limit);
        }
    }

private:

    unique_ptr<Bus> bus;

    unique_ptr<InitiatorExecutor> initiator_executor;

    unique_ptr<SgAdapter> sg_adapter;

    logger &s2pexec_logger;

    bool is_sg = false;

    // The SCSI ExecuteOperation custom command supports a byte count of up to 65535 bytes
    static constexpr int BUFFER_SIZE = 65535;
};
