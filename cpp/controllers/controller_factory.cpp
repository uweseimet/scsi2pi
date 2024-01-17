//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_controller.h"
#include "sasi_controller.h"
#include "controller_factory.h"

using namespace std;

shared_ptr<AbstractController> ControllerFactory::CreateController(Bus &bus, int id) const
{
    shared_ptr<AbstractController> controller;
    if (is_sasi) {
        controller = make_shared<SasiController>(bus, id, GetSasiLunMax());
    }
    else {
        controller = make_shared<ScsiController>(bus, id, GetScsiLunMax());
    }

    controller->Init();

    return controller;
}

bool ControllerFactory::AttachToController(Bus &bus, int id, shared_ptr<PrimaryDevice> device)
{
    if (auto controller = FindController(id); controller) {
        if (device->GetLun() > GetLunMax() || controller->HasDeviceForLun(device->GetLun())) {
            return false;
        }

        return controller->AddDevice(device);
    }

    // If this is LUN 0 create a new controller
    if (!device->GetLun()) {
        if (auto controller = CreateController(bus, id); controller->AddDevice(device)) {
            controllers[id] = controller;

            return true;
        }
    }

    return false;
}

bool ControllerFactory::DeleteController(const AbstractController &controller)
{
    for (const auto &device : controller.GetDevices()) {
        device->CleanUp();
    }

    return controllers.erase(controller.GetTargetId()) == 1;
}

bool ControllerFactory::DeleteAllControllers()
{
    bool has_controller = false;

    unordered_set<shared_ptr<AbstractController>> values;
    ranges::transform(controllers, inserter(values, values.begin()), [](const auto &controller) {
            return controller.second;
        });

    for (const auto &controller : values) {
        DeleteController(*controller);
        has_controller = true;
    }

    assert(controllers.empty());

    return has_controller;
}

AbstractController::shutdown_mode ControllerFactory::ProcessOnController(int id_data) const
{
    if (const auto &it = ranges::find_if(controllers, [&](const auto &c) {
        return (id_data & (1 << c.first));
    }); it != controllers.end()) {
        (*it).second->ProcessOnController(id_data);

        return (*it).second->GetShutdownMode();
    }

    return AbstractController::shutdown_mode::NONE;
}

shared_ptr<AbstractController> ControllerFactory::FindController(int target_id) const
{
    const auto &it = controllers.find(target_id);
    return it == controllers.end() ? nullptr : it->second;
}

bool ControllerFactory::HasController(int target_id) const
{
    return controllers.contains(target_id);
}

unordered_set<shared_ptr<PrimaryDevice>> ControllerFactory::GetAllDevices() const
{
    unordered_set<shared_ptr<PrimaryDevice>> devices;

    for (const auto& [_, controller] : controllers) {
        const auto &d = controller->GetDevices();
        devices.insert(d.begin(), d.end());
    }

    return devices;
}

bool ControllerFactory::HasDeviceForIdAndLun(int id, int lun) const
{
    return GetDeviceForIdAndLun(id, lun) != nullptr;
}

shared_ptr<PrimaryDevice> ControllerFactory::GetDeviceForIdAndLun(int id, int lun) const
{
    if (const auto &controller = FindController(id); controller) {
        return controller->GetDeviceForLun(lun);
    }

    return nullptr;
}
