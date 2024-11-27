//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controller_factory.h"
#include "base/primary_device.h"
#include "controller.h"

using namespace std;

bool ControllerFactory::AttachToController(Bus &bus, int id, shared_ptr<PrimaryDevice> device)
{
    if (const auto &it = controllers.find(id); it != controllers.end()) {
        return it->second->AddDevice(device);
    }

    // If this is LUN 0 create a new controller
    if (!device->GetLun()) {
        if (auto controller = make_shared<Controller>(bus, id); controller->AddDevice(device)) {
            controller->Init();
            controller->SetScriptGenerator(script_generator);

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

bool ControllerFactory::SetScriptFile(const string &filename)
{
    auto generator = make_shared<ScriptGenerator>();
    if (!generator->CreateFile(filename)) {
        return false;
    }

    script_generator = generator;

    return true;
}

shutdown_mode ControllerFactory::ProcessOnController(int ids) const
{
    if (const auto &it = ranges::find_if(controllers, [&](const auto &c) {
        return (ids & (1 << c.first));
    }); it != controllers.end()) {
        return (*it).second->ProcessOnController(ids);
    }

    return shutdown_mode::none;
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

shared_ptr<PrimaryDevice> ControllerFactory::GetDeviceForIdAndLun(int id, int lun) const
{
    const auto &it = controllers.find(id);
    return it == controllers.end() ? nullptr : it->second->GetDeviceForLun(lun);
}
