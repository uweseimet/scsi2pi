//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/sinks/stdout_color_sinks.h>
#include "base/primary_device.h"
#include "buses/bus.h"
#include "script_generator.h"

using namespace s2p_util;

AbstractController::AbstractController(Bus &b, int id, const S2pFormatter &f) : bus(b), target_id(id), formatter(f)
{
    controller_logger = CreateLogger(fmt::format("[s2p] (ID {})", id));
}

void AbstractController::CleanUp() const
{
    for (const auto& [_, lun] : luns) {
        lun->CleanUp();
    }
}

void AbstractController::Reset()
{
    SetPhase(BusPhase::BUS_FREE);

    offset = 0;
    remaining_length = 0;
    current_length = 0;
    chunk_size = 0;

    status = StatusCode::GOOD;

    initiator_id = UNKNOWN_INITIATOR_ID;

    for (const auto& [_, lun] : luns) {
        lun->Reset();
    }

    bus.Reset();
}

void AbstractController::SetScriptGenerator(shared_ptr<ScriptGenerator> s)
{
    script_generator = s;
}

void AbstractController::AddCdbToScript()
{
    if (script_generator) {
        script_generator->AddCdb(target_id, GetEffectiveLun(), cdb);
    }
}

void AbstractController::AddDataToScript(span<const uint8_t> data) const
{
    if (script_generator) {
        script_generator->AddData(data);
    }
}

void AbstractController::SetCurrentLength(int length)
{
    if (length > static_cast<int>(buffer.size())) {
        buffer.resize(length);
    }

    current_length = length;
}

void AbstractController::SetTransferSize(int length, int size)
{
    // The total number of bytes to transfer for the current command
    remaining_length = length;

    // The number of bytes to transfer in a single chunk
    chunk_size = length < size ? length : size;
}

void AbstractController::UpdateTransferLength(int length)
{
    remaining_length -= length;

    assert(remaining_length >= 0);
    if (remaining_length < 0) {
        remaining_length = 0;
    }

    if (remaining_length < chunk_size) {
        chunk_size = remaining_length;
    }
}

void AbstractController::UpdateOffsetAndLength()
{
    offset += current_length;
    current_length = 0;
}

void AbstractController::CopyToBuffer(const void *src, size_t size) // NOSONAR Any kind of source data is permitted
{
    SetCurrentLength(static_cast<int>(size));

    memcpy(buffer.data(), src, size);
}

unordered_set<shared_ptr<PrimaryDevice>> AbstractController::GetDevices() const
{
    unordered_set<shared_ptr<PrimaryDevice>> devices;

    // "luns | views:values" is not supported by the bullseye compiler
    ranges::transform(luns, inserter(devices, devices.begin()), [](const auto &l) {return l.second;});

    return devices;
}

shared_ptr<PrimaryDevice> AbstractController::GetDeviceForLun(int lun) const
{
    const auto &it = luns.find(lun);
    return it == luns.end() ? nullptr : it->second;
}

ShutdownMode AbstractController::ProcessOnController(int ids)
{
    if (const int ids_without_target = ids - (1 << target_id); ids_without_target) {
        initiator_id = 0;
        while (!(ids_without_target & (1 << initiator_id))) {
            ++initiator_id;
        }
        LogTrace("++++ Starting processing for initiator ID " + to_string(initiator_id));
    }
    else {
        initiator_id = UNKNOWN_INITIATOR_ID;
        LogTrace("++++ Starting processing for unknown initiator ID");
    }

    while (Process()) {
        // Handle bus phases until the bus is free for the next command
    }

    if (script_generator) {
        script_generator->WriteEol();
    }

    return shutdown_mode;
}

bool AbstractController::AddDevice(shared_ptr<PrimaryDevice> device)
{
    const int lun = device->GetLun();
    if (lun < 0 || lun >= (device->GetType() == SAHD ? 2 : 32) || luns.contains(lun) || device->GetController()) {
        return false;
    }

    luns[lun] = device;

    device->SetController(this);

    return true;
}

bool AbstractController::RemoveDevice(PrimaryDevice &device)
{
    device.CleanUp();

    return luns.erase(device.GetLun()) == 1;
}

void AbstractController::LogTrace(const string &s) const
{
    controller_logger->trace(s);
}

void AbstractController::LogDebug(const string &s) const
{
    controller_logger->debug(s);
}

void AbstractController::LogWarn(const string &s) const
{
    controller_logger->warn(s);
}
