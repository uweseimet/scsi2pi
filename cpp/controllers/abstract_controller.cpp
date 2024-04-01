//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cstring>
#include "base/primary_device.h"

AbstractController::AbstractController(Bus &bus, int target_id, int max_luns) : bus(bus), target_id(target_id), max_luns(
    max_luns)
{
    // The initial buffer size is the size of the biggest supported sector
    buffer.resize(4096);

    device_logger.SetIdAndLun(target_id, -1);
}

void AbstractController::CleanUp() const
{
    for (const auto& [_, device] : luns) {
        device->CleanUp();
    }
}

void AbstractController::Reset()
{
    SetPhase(bus_phase::busfree);

    offset = 0;
    total_length = 0;
    current_length = 0;
    chunk_size = 0;

    status = status_code::good;

    initiator_id = UNKNOWN_INITIATOR_ID;

    for (const auto& [_, device] : luns) {
        device->Reset();
    }

    bus.Reset();
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
    // The total number of bytes to transfer for the current SCSI/SASI command
    total_length = length;

    // The number of bytes to transfer in a single chunk
    chunk_size = size;
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

AbstractController::shutdown_mode AbstractController::ProcessOnController(int ids)
{
    device_logger.SetIdAndLun(target_id, -1);

    if (int ids_without_target = ids - (1 << target_id); ids_without_target) {
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

    return sh_mode;
}

bool AbstractController::AddDevice(shared_ptr<PrimaryDevice> device)
{
    const int lun = device->GetLun();
    if (lun < 0 || lun >= max_luns || GetDeviceForLun(lun) || device->GetController()) {
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
