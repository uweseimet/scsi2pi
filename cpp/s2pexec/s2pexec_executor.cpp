//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pexec_executor.h"
#include "buses/bus_factory.h"
#include "initiator/initiator_util.h"

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

    use_sg = error.empty();

    return error;
#else
    return "";
#endif
}

string S2pExecExecutor::Init(int id, const string &name, bool in_process)
{
    if (!bus) {
        bus = BusFactory::Instance().CreateBus(false, in_process, name);
        if (!bus) {
            return "Can't initialize bus";
        }

        if (!in_process && !bus->IsRaspberryPi()) {
            return "No RaSCSI/PiSCSI board found";
        }

        initiator_executor = make_unique<InitiatorExecutor>(*bus, id, s2pexec_logger);
    }

    use_sg = false;

    return "";
}

void S2pExecExecutor::CleanUp()
{
    if (!use_sg && bus) {
        bus->CleanUp();
    }

#ifdef __linux__
    if (use_sg && sg_adapter) {
        sg_adapter->CleanUp();
    }
#endif

    use_sg = false;
}

void S2pExecExecutor::ResetBus()
{
    if (!use_sg && bus) {
        initiator_util::ResetBus(*bus);
    }
}

int S2pExecExecutor::ExecuteCommand(vector<uint8_t> &cdb, vector<uint8_t> &buf, int timeout, bool log)
{
#ifdef __linux__
    if (use_sg) {
        return sg_adapter->SendCommand(cdb, buf, static_cast<int>(buf.size()), timeout).status;
    }
#endif

    return initiator_executor->Execute(cdb, buf, static_cast<int>(buf.size()), timeout, log);
}

tuple<sense_key, asc, int> S2pExecExecutor::GetSenseData() const
{
#ifdef __linux__
    if (use_sg) {
        array<uint8_t, 14> sense_data;
        vector<uint8_t> cdb(6);
        cdb[0] = static_cast<uint8_t>(scsi_command::request_sense);
        cdb[4] = static_cast<uint8_t>(sense_data.size());

        sg_adapter->SendCommand(cdb, sense_data, static_cast<int>(sense_data.size()), 1);

        return {static_cast<sense_key>(static_cast<int>(sense_data[0]) & 0x0f), static_cast<asc>(sense_data[12]), sense_data[13]};
    }
#endif

    return initiator_util::GetSenseData(*initiator_executor);
}

int S2pExecExecutor::GetByteCount() const
{
    return use_sg ? sg_adapter->GetByteCount() : initiator_executor->GetByteCount();
}

void S2pExecExecutor::SetTarget(int id, int lun, bool sasi)
{
    if (initiator_executor) {
        initiator_executor->SetTarget(id, lun, sasi);
    }
}
