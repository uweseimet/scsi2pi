//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pexec_executor.h"
#include "buses/bus_factory.h"

string S2pExecExecutor::Init(const string &device)
{
#ifdef __linux__
    if (sg_adapter) {
        sg_adapter->CleanUp();
    }
    else {
        sg_adapter = make_unique<SgAdapter>(s2pexec_logger);
    }

    const string &error = sg_adapter->Init(device);

    is_sg = error.empty();

    return error;
#else
    return "";
#endif
}

string S2pExecExecutor::Init(int id, const string &name, bool in_process)
{
    if (!bus) {
        bus = bus_factory::CreateBus(false, in_process, name, false);
        if (!bus) {
            return "Can't initialize bus";
        }

        if (!in_process && !bus->IsRaspberryPi()) {
            return "No RaSCSI/PiSCSI board found";
        }

        initiator_executor = make_unique<InitiatorExecutor>(*bus, id, s2pexec_logger);
    }

    is_sg = false;

    return "";
}

void S2pExecExecutor::CleanUp()
{
    if (!is_sg && bus) {
        bus->CleanUp();
    }

#ifdef __linux__
    if (is_sg && sg_adapter) {
        sg_adapter->CleanUp();
    }
#endif

    is_sg = false;
}

void S2pExecExecutor::ResetBus()
{
    if (!is_sg && bus) {
        initiator_util::ResetBus(*bus);
    }
}

int S2pExecExecutor::ExecuteCommand(vector<uint8_t> &cdb, vector<uint8_t> &buf, int timeout, bool log)
{
#ifdef __linux__
    if (is_sg) {
        return sg_adapter->SendCommand(cdb, buf, static_cast<int>(buf.size()), timeout).status;
    }
#endif

    return initiator_executor->Execute(cdb, buf, static_cast<int>(buf.size()), timeout, log);
}

tuple<SenseKey, Asc, int> S2pExecExecutor::GetSenseData() const
{
#ifdef __linux__
    if (is_sg) {
        array<uint8_t, 14> sense_data;
        vector<uint8_t> cdb(6);
        cdb[0] = static_cast<uint8_t>(ScsiCommand::REQUEST_SENSE);
        cdb[4] = static_cast<uint8_t>(sense_data.size());

        sg_adapter->SendCommand(cdb, sense_data, static_cast<int>(sense_data.size()), 1);

        return {static_cast<SenseKey>(static_cast<int>(sense_data[0]) & 0x0f), static_cast<Asc>(sense_data[12]), sense_data[13]};
    }
#endif

    return initiator_util::GetSenseData(*initiator_executor);
}

int S2pExecExecutor::GetByteCount() const
{
    return is_sg ? sg_adapter->GetByteCount() : initiator_executor->GetByteCount();
}

void S2pExecExecutor::SetTarget(int id, int lun, bool sasi)
{
    if (initiator_executor) {
        initiator_executor->SetTarget(id, lun, sasi);
    }
}
