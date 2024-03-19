//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controller.h"
#include "controller_factory.h"

using namespace std;

shared_ptr<AbstractController> ControllerFactory::CreateController(Bus &bus, int id) const
{
    shared_ptr<AbstractController> controller = make_shared<Controller>(bus, id,
        is_sasi ? GetSasiLunMax() : GetScsiLunMax());
    controller->Init();

    return controller;
}

bool ControllerFactory::AttachToController(Bus &bus, int id, shared_ptr<PrimaryDevice> device)
{
    if ((!is_sasi && device->GetType() == PbDeviceType::SAHD) || (is_sasi && device->GetType() != PbDeviceType::SAHD)) {
        return false;
    }

    if (auto controller = FindController(id); controller) {
        if (device->GetLun() > GetLunMax() || controller->GetDeviceForLun(device->GetLun())) {
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
    controller.CleanUp();

    return controllers.erase(controller.GetTargetId()) == 1;
}

bool ControllerFactory::DeleteAllControllers()
{
    if (controllers.empty()) {
        return false;
    }

    for (auto it = controllers.cbegin(); it != controllers.cend();) {
        (*it).second->CleanUp();
        controllers.erase(it++);
    }

    return true;
}

AbstractController::shutdown_mode ControllerFactory::ProcessOnController(int ids) const
{
    if (const auto &it = ranges::find_if(controllers, [&](const auto &c) {
        return (ids & (1 << c.first));
    }); it != controllers.end()) {
        return (*it).second->ProcessOnController(ids);
    }

    return AbstractController::shutdown_mode::none;
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
        devices.insert(d.cbegin(), d.cend());
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
