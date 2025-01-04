//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controller_factory.h"
#include "base/primary_device.h"
#include "controller.h"
#include "script_generator.h"

bool ControllerFactory::AttachToController(Bus &bus, int id, shared_ptr<PrimaryDevice> device)
{
    if (const auto &it = controllers.find(id); it != controllers.end()) {
        const bool status = it->second->AddDevice(device);

        device->GetLogger().set_level(log_level);
        device->GetLogger().set_pattern(log_pattern);

        return status;
    }

    // If this is LUN 0 create a new controller
    if (!device->GetLun()) {
        if (auto controller = make_shared<Controller>(bus, id, formatter); controller->AddDevice(device)) {
            controller->GetLogger().set_level(log_level);
            controller->GetLogger().set_pattern(log_pattern);

            controller->Init();
            controller->SetScriptGenerator(script_generator);

            controllers[id] = controller;

            device->GetLogger().set_level(log_level);
            device->GetLogger().set_pattern(log_pattern);

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

ShutdownMode ControllerFactory::ProcessOnController(int ids) const
{
    if (const auto &it = ranges::find_if(controllers, [&ids](const auto &c) {
        return (ids & (1 << c.first));
    }); it != controllers.end()) {
        return (*it).second->ProcessOnController(ids);
    }

    return ShutdownMode::NONE;
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

void ControllerFactory::SetLogLevel(int id, int lun, level::level_enum level)
{
    log_level = level;

    for (const auto &device : GetAllDevices()) {
        if (id == -1 || (device->GetId() == id && (lun == -1 || device->GetLun() == lun))) {
            device->GetController()->GetLogger().set_level(log_level);
            device->GetController()->GetLogger().set_pattern(log_pattern);
            device->GetLogger().set_level(log_level);
            device->GetLogger().set_pattern(log_pattern);
        }
        else {
            device->GetController()->GetLogger().set_level(level::level_enum::off);
            device->GetLogger().set_level(level::level_enum::off);
        }
    }
}

void ControllerFactory::SetLogPattern(string_view pattern)
{
    log_pattern = pattern;
}
