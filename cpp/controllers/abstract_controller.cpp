//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cstring>
#include "base/primary_device.h"

using namespace scsi_defs;

AbstractController::AbstractController(Bus &bus, int target_id, int max_luns) : bus(bus), target_id(target_id), max_luns(
    max_luns)
{
    // The initial buffer size is the size of the biggest supported sector
    ctrl.buffer.resize(4096);

    device_logger.SetIdAndLun(target_id, -1);
}

void AbstractController::SetCurrentLength(size_t length)
{
    if (length > ctrl.buffer.size()) {
        ctrl.buffer.resize(length);
    }

    ctrl.current_length = static_cast<int>(length);
}

void AbstractController::CopyToBuffer(const void *src, size_t size) // NOSONAR Any kind of source data is permitted
{
    SetCurrentLength(size);

    memcpy(ctrl.buffer.data(), src, size);
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

void AbstractController::Reset()
{
    SetPhase(phase_t::busfree);

    ctrl = { };

    // Reset all LUNs
    for (const auto& [_, device] : luns) {
        device->Reset();
    }

    GetBus().Reset();
}

void AbstractController::ProcessOnController(int id_data)
{
    device_logger.SetIdAndLun(GetTargetId(), -1);

    const int initiator_id = ExtractInitiatorId(id_data);
    if (initiator_id != UNKNOWN_INITIATOR_ID) {
        LogTrace(fmt::format("++++ Starting processing for initiator ID {}", initiator_id));
    }
    else {
        LogTrace("++++ Starting processing for unknown initiator ID");
    }

    while (Process(initiator_id)) {
        // Handle bus phases until the bus is free for the next command
    }
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

int AbstractController::ExtractInitiatorId(int id_data) const
{
    if (const int id_data_without_target = id_data - (1 << target_id); id_data_without_target) {
        return static_cast<int>(log2(id_data_without_target & -id_data_without_target));
    }

    return UNKNOWN_INITIATOR_ID;
}
