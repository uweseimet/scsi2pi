//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2020-2023 Contributors to the PiSCSI project
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <netinet/in.h>
#include <csignal>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "shared/s2p_version.h"
#include "shared_protobuf/protobuf_util.h"
#ifdef BUILD_SCHS
#include "devices/host_services.h"
#endif
#include "s2p_parser.h"
#include "s2p_core.h"

using namespace std;
using namespace spdlog;
using namespace s2p_interface;
using namespace s2p_util;
using namespace protobuf_util;
using namespace scsi_defs;

bool S2p::InitBus(bool in_process, bool is_sasi)
{
    bus_factory = make_unique<BusFactory>();

    bus = bus_factory->CreateBus(true, in_process);
    if (!bus) {
        return false;
    }

    controller_factory = make_shared<ControllerFactory>(is_sasi);

    executor = make_unique<CommandExecutor>(*bus, controller_factory);

    dispatcher = make_shared<CommandDispatcher>(s2p_image, response, *executor);

    return true;
}

void S2p::CleanUp()
{
    if (service_thread.IsRunning()) {
        service_thread.Stop();
    }

    executor->DetachAll();

    // TODO Check why there are rare cases where bus is NULL on a remote interface shutdown
    // even though it is never set to NULL anywhere
    if (bus) {
        bus->CleanUp();
    }
}

void S2p::ReadAccessToken(const path &filename)
{
    if (error_code error; !is_regular_file(filename, error)) {
        throw parser_exception("Access token file '" + filename.string() + "' must be a regular file");
    }

    if (struct stat st; stat(filename.c_str(), &st) || st.st_uid || st.st_gid) {
        throw parser_exception("Access token file '" + filename.string() + "' must be owned by root");
    }

    if (const auto perms = filesystem::status(filename).permissions();
    (perms & perms::group_read) != perms::none || (perms & perms::others_read) != perms::none ||
        (perms & perms::group_write) != perms::none || (perms & perms::others_write) != perms::none) {
        throw parser_exception("Access token file '" + filename.string() + "' must be readable by root only");
    }

    ifstream token_file(filename);
    if (token_file.fail()) {
        throw parser_exception("Can't open access token file '" + filename.string() + "'");
    }

    getline(token_file, access_token);
    if (token_file.fail()) {
        throw parser_exception("Can't read access token file '" + filename.string() + "'");
    }

    if (access_token.empty()) {
        throw parser_exception("Access token file '" + filename.string() + "' must not be empty");
    }
}

void S2p::LogDevices(string_view devices) const
{
    stringstream ss(devices.data());
    string line;

    while (getline(ss, line)) {
        spdlog::info(line);
    }
}

void S2p::TerminationHandler(int)
{
    instance->CleanUp();

    // Process will terminate automatically
}

bool S2p::HandleDeviceListChange(const CommandContext &context, PbOperation operation) const
{
    // ATTACH and DETACH return the resulting device list
    if (operation == ATTACH || operation == DETACH) {
        // A command with an empty device list is required here in order to return data for all devices
        PbCommand command;
        PbResult result;
        response.GetDevicesInfo(executor->GetAllDevices(), result, command, s2p_image.GetDefaultFolder());
        context.WriteResult(result);
        return result.status();
    }

    return true;
}

int S2p::run(span<char*> args, bool in_process)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // The -v option shall result in no other action except displaying the version
    if (ranges::find_if(args, [](const char *arg) {return !strcmp(arg, "-v");}) != args.end()) {
        cout << GetVersionString() << '\n';
        return EXIT_SUCCESS;
    }

    s2p_parser.Banner(args, false);

    bool is_sasi = false;
    int port;
    try {
        const auto &properties = s2p_parser.ParseArguments(args, is_sasi);
        property_handler.Init(properties.at(PropertyHandler::PROPERTY_FILE), properties);

        if (const string &log_level = property_handler.GetProperty(PropertyHandler::LOG_LEVEL);
        !CommandDispatcher::SetLogLevel(log_level)) {
            throw parser_exception("Invalid log level '" + log_level + "'");
        }

        // Log the properties (on trace level) *after* the log level has been set
        LogProperties();

        if (const string &image_folder = property_handler.GetProperty(PropertyHandler::IMAGE_FOLDER); !image_folder.empty()) {
            if (const string error = s2p_image.SetDefaultFolder(image_folder); !error.empty()) {
                throw parser_exception(error);
            }
        }

        if (const string &scan_depth = property_handler.GetProperty(PropertyHandler::SCAN_DEPTH); !scan_depth.empty()) {
            if (int depth; !GetAsUnsignedInt(scan_depth, depth)) {
                throw parser_exception(
                    "Invalid image file scan depth " + property_handler.GetProperty(PropertyHandler::PORT));
            }
            else {
                s2p_image.SetDepth(depth);
            }
        }

        if (const string &p = property_handler.GetProperty(PropertyHandler::PORT); !GetAsUnsignedInt(p, port)
            || port <= 0 || port > 65535) {
            throw parser_exception("Invalid port " + p + ", port must be between 1 and 65535");
        }
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    if (!InitBus(in_process, is_sasi)) {
        cerr << "Error: Can't initialize bus" << endl;
        return EXIT_FAILURE;
    }

    if (const string error = executor->SetReservedIds(property_handler.GetProperty(PropertyHandler::RESERVED_IDS)); !error.empty()) {
        cerr << "Error: " << error << endl;
        CleanUp();
        return EXIT_FAILURE;
    }

    if (const string &token_file = property_handler.GetProperty(PropertyHandler::TOKEN_FILE); !token_file.empty()) {
        ReadAccessToken(path(token_file));
    }

    if (const string error = service_thread.Init([this](CommandContext &context) {
        return ExecuteCommand(context);
    }, port); !error.empty()) {
        cerr << "Error: " << error << endl;
        CleanUp();
        return EXIT_FAILURE;
    }

    try {
        CreateDevices();
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        CleanUp();
        return EXIT_FAILURE;
    }

    // Display and log the device list
    PbServerInfo server_info;
    response.GetDevices(executor->GetAllDevices(), server_info, s2p_image.GetDefaultFolder());
    const vector<PbDevice> &devices = { server_info.devices_info().devices().begin(),
        server_info.devices_info().devices().end() };
    const string device_list = ListDevices(devices);
    LogDevices(device_list);
    cout << device_list << flush;

    if (!bus_factory->IsRaspberryPi()) {
        cout << "Note: No board hardware support, only client interface calls are supported\n" << flush;
    }

    SetUpEnvironment();

    service_thread.Start();

    ProcessScsiCommands();

    return EXIT_SUCCESS;
}

void S2p::SetUpEnvironment()
{
    instance = this;

    // Signal handler to detach all devices on a KILL or TERM signal
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);
}

void S2p::LogProperties() const
{
    spdlog::trace("Effective properties:");
    for (const auto& [k, v] : property_handler.GetProperties()) {
        spdlog::trace(fmt::format("  {0}={1}", k, v));
    }
}

void S2p::CreateDevices()
{
    PbCommand command;
    PbDeviceDefinition device_definition;
    PbDeviceDefinition *device;

    // The properties are sorted, i.e. there is a contiguous block for each device
    int id = -1;
    int lun = -1;
    for (const auto& [key, value] : property_handler.GetProperties()) {
        if (!key.starts_with("device.")) {
            continue;
        }

        const auto &key_components = Split(key, '.', 3);
        if (key_components.size() < 3) {
            throw parser_exception(fmt::format("Invalid device definition '{}'", key));
        }

        const auto &id_and_lun = key_components[1];
        if (const string error = SetIdAndLun(ControllerFactory::GetIdMax(), ControllerFactory::GetLunMax(),
            device_definition, id_and_lun); !error.empty()) {
            throw parser_exception(error);
        }

        // Create a new device at the start of a new device block
        if (id != device_definition.id() || lun != device_definition.unit()) {
            device = command.add_devices();
            id = device_definition.id();
            lun = device_definition.unit();
            device->set_id(id);
            device->set_unit(lun);
        }

        if (key_components[2] == "type") {
            device->set_type(ParseDeviceType(value));
        }
        else if (key_components[2] == "block_size") {
            if (int block_size; !GetAsUnsignedInt(value, block_size)) {
                throw parser_exception(fmt::format("Invalid block size: {}", value));
            }
            else {
                device->set_block_size(block_size);
            }
        }
        else if (key_components[2] == "product_data") {
            SetProductData(*device, value);
        }
        else if (key_components[2] == "params") {
            ParseParameters(*device, value);
        }
        else {
            throw parser_exception(fmt::format("Unknown device definition key: '{}'", key_components[2]));
        }
    }

    if (command.devices_size()) {
        command.set_operation(ATTACH);

        if (const CommandContext context(command, s2p_image.GetDefaultFolder(),
            property_handler.GetProperty(PropertyHandler::LOCALE)); !executor->ProcessCmd(context)) {
            throw parser_exception("Error: Can't attach devices");
        }

#ifdef BUILD_SCHS
        // Ensure that all host services have a dispatcher
        for (auto d : controller_factory->GetAllDevices()) {
            if (auto host_services = dynamic_pointer_cast<HostServices>(d); host_services != nullptr) {
                host_services->SetDispatcher(dispatcher);
            }
        }
#endif
    }
}

PbDeviceType S2p::ParseDeviceType(const string &value)
{
    string t;
    ranges::transform(value, back_inserter(t), ::toupper);
    if (PbDeviceType type; PbDeviceType_Parse(t, &type)) {
        return type;
    }

    throw parser_exception("Illegal device type '" + value + "'");
}

void S2p::ProcessScsiCommands()
{
    while (service_thread.IsRunning()) {
        // Only process the SCSI command if the bus is not busy and no other device responded
        // TODO There may be something wrong with the SEL/BSY handling, see PhaseExecutor/Arbitration
        if (bus->WaitForSelection() && WaitForNotBusy()) {
            scoped_lock<mutex> lock(executor->GetExecutionLocker());

            // Process command on the responsible controller based on the current initiator and target ID
            if (const auto shutdown_mode = controller_factory->ProcessOnController(bus->GetDAT());
            shutdown_mode != AbstractController::shutdown_mode::NONE) {
                // When the bus is free SCSI2Pi or the Pi may be shut down.
                dispatcher->ShutDown(shutdown_mode);
            }
        }
    }
}

bool S2p::ExecuteCommand(CommandContext &context)
{
    if (!access_token.empty() && access_token != GetParam(context.GetCommand(), "token")) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_AUTHENTICATION, UNAUTHORIZED);
    }

    context.SetDefaultFolder(s2p_image.GetDefaultFolder());
    PbResult result;
    const bool status = dispatcher->DispatchCommand(context, result, "");
    if (status && context.GetCommand().operation() == PbOperation::SHUT_DOWN) {
        CleanUp();
        return false;
    }

    return status;
}

bool S2p::WaitForNotBusy() const
{
    // Wait up to 3 s for BSY to be released, signalling the end of the ARBITRATION phase
    if (bus->GetBSY()) {
        const auto now = chrono::steady_clock::now();
        while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3) {
            bus->Acquire();
            if (!bus->GetBSY()) {
                return true;
            }
        }

        return false;
    }

    return true;
}
