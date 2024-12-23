//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "initiator/initiator_util.h"
#include "shared/sg_adapter.h"

using namespace std;

class S2pExecExecutor
{

public:

    S2pExecExecutor(Bus &bus, int id, logger &l) : initiator_executor(make_unique<InitiatorExecutor>(bus, id, l)), sg_adapter(
        make_unique<SgAdapter>(l))
    {
    }
    ~S2pExecExecutor() = default;

    string Init(const string&);

    int ExecuteCommand(vector<uint8_t>&, vector<uint8_t>&, int, bool);

    tuple<sense_key, asc, int> GetSenseData() const;

    int GetByteCount() const;

    void SetTarget(int, int, bool);

private:

    unique_ptr<InitiatorExecutor> initiator_executor;

    unique_ptr<SgAdapter> sg_adapter;

    bool use_sg = false;

    int length = 0;

    // The SCSI ExecuteOperation custom command supports a byte count of up to 65535 bytes
    static constexpr int BUFFER_SIZE = 65535;
};
