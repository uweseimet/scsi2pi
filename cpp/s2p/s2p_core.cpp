//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2020-2023 Contributors to the PiSCSI project
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_core.h"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include "base/device_factory.h"
#include "buses/bus_factory.h"
#include "command/command_context.h"
#include "command/command_image_support.h"
#include "command/command_response.h"
#ifdef BUILD_SCHS
#include "devices/host_services.h"
#endif
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_version.h"
#include "s2p_parser.h"

using namespace spdlog;
using namespace s2p_util;
using namespace protobuf_util;

bool S2p::InitBus(bool in_process, bool log_signals)
{
    bus = BusFactory::Instance().CreateBus(true, in_process, log_signals);
    if (!bus) {
        return false;
    }

    executor = make_unique<CommandExecutor>(*bus, controller_factory);

    dispatcher = make_shared<CommandDispatcher>(*executor);

    return true;
}

void S2p::CleanUp()
{
    if (service_thread.IsRunning()) {
        service_thread.Stop();
    }

    executor->DetachAll();

    // TODO Check why there are rare cases where bus is NULL on a remote interface shutdown
    // even though it is never set to NULL anywhere. This looks like a race condition.
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

void S2p::LogDevices(const string &devices) const
{
    stringstream ss(devices);
    string line;

    while (getline(ss, line)) {
        info(line);
    }
}

void S2p::TerminationHandler(int)
{
    instance->CleanUp();

    // Process will terminate automatically
}

int S2p::Run(span<char*> args, bool in_process, bool log_signals)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // The --version/-v option shall result in no other action except displaying the version
    if (ranges::find_if(args, [](const char *arg) {return !strcmp(arg, "-v") || !strcmp(arg, "--version");})
        != args.end()) {
        cout << GetVersionString() << '\n';
        return EXIT_SUCCESS;
    }

    S2pParser parser;

    parser.Banner(false);

    bool ignore_conf = false;

    property_map properties;
    try {
        properties = parser.ParseArguments(args, ignore_conf);
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    int port;
    if (!ParseProperties(properties, port, ignore_conf)) {
        return EXIT_FAILURE;
    }

    if (const string &error = MapExtensions(); !error.empty()) {
        cerr << "Error: " << error << endl;
        return EXIT_FAILURE;
    }

    if (!InitBus(in_process, log_signals)) {
        cerr << "Error: Can't initialize bus" << endl;
        return EXIT_FAILURE;
    }

    if (const string &reserved_ids = property_handler.GetProperty(PropertyHandler::RESERVED_IDS); !reserved_ids.empty()) {
        if (const string &error = executor->SetReservedIds(reserved_ids); !error.empty()) {
            cerr << "Error: " << error << endl;
            CleanUp();
            return EXIT_FAILURE;
        }
    }

    if (const string &token_file = property_handler.GetProperty(PropertyHandler::TOKEN_FILE); !token_file.empty()) {
        ReadAccessToken(path(token_file));
    }

    if (const string &error = service_thread.Init([this](CommandContext &context) {
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
    CommandResponse response;
    response.GetDevices(executor->GetAllDevices(), server_info);
    const vector<PbDevice> &devices = { server_info.devices_info().devices().cbegin(),
        server_info.devices_info().devices().cend() };
    const string device_list = ListDevices(devices);
    LogDevices(device_list);

    // Show the device list only once
    if (get_level() > level::info) {
        cout << device_list << flush;
    }

    if (!in_process && !bus->IsRaspberryPi()) {
        cout << "Note: No board hardware support, only client interface calls are supported\n" << flush;
    }

    SetUpEnvironment();

    service_thread.Start();

    // Signal the in-process client that s2p is ready
    if (in_process) {
        bus->CleanUp();
    }

    ProcessScsiCommands();

    return EXIT_SUCCESS;
}

bool S2p::ParseProperties(const property_map &properties, int &port, bool ignore_conf)
{
    const auto &property_files = properties.find(PropertyHandler::PROPERTY_FILES);
    try {
        property_handler.Init(property_files != properties.end() ? property_files->second : "", properties,
            ignore_conf);

        if (const string &log_level = property_handler.GetProperty(PropertyHandler::LOG_LEVEL, "info");
        !CommandDispatcher::SetLogLevel(log_level)) {
            throw parser_exception("Invalid log level: '" + log_level + "'");
        }

        if (const string &log_pattern = property_handler.GetProperty(PropertyHandler::LOG_PATTERN); !log_pattern.empty()) {
            set_pattern(log_pattern);
        }

        // Log the properties (on trace level) *after* the log level has been set
        LogProperties();

        if (const string &image_folder = property_handler.GetProperty(PropertyHandler::IMAGE_FOLDER); !image_folder.empty()) {
            if (const string &error = CommandImageSupport::Instance().SetDefaultFolder(image_folder); !error.empty()) {
                throw parser_exception(error);
            }
        }

        if (const string &scan_depth = property_handler.GetProperty(PropertyHandler::SCAN_DEPTH, "1"); !scan_depth.empty()) {
            if (int depth; !GetAsUnsignedInt(scan_depth, depth)) {
                throw parser_exception(
                    "Invalid image file scan depth " + property_handler.GetProperty(PropertyHandler::SCAN_DEPTH));
            }
            else {
                CommandImageSupport::Instance().SetDepth(depth);
            }
        }

        if (const string &p = property_handler.GetProperty(PropertyHandler::PORT, "6868"); !GetAsUnsignedInt(p, port)
            || port <= 0 || port > 65535) {
            throw parser_exception("Invalid port: '" + p + "', port must be between 1 and 65535");
        }
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return false;
    }

    return true;
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

string S2p::MapExtensions() const
{
    for (const auto& [key, value] : property_handler.GetProperties("extensions.")) {
        const auto &components = Split(key, '.');
        if (components.size() != 2) {
            return "Invalid extension mapping: '" + key + "'";
        }

        PbDeviceType type = UNDEFINED;
        if (PbDeviceType_Parse(ToUpper(components[1]), &type) && type == UNDEFINED) {
            continue;
        }

        for (const string &extension : Split(value, ',')) {
            if (!DeviceFactory::Instance().AddExtensionMapping(extension, type)) {
                return "Duplicate extension mapping for extension '" + extension + "'";
            }
        }
    }

    return "";
}

void S2p::LogProperties() const
{
    trace("Effective startup properties:");
    for (const auto& [k, v] : property_handler.GetProperties()) {
        trace("  {0}={1}", k, v);
    }
}

void S2p::CreateDevices()
{
    PbCommand command;
    PbDeviceDefinition device_definition;
    PbDeviceDefinition *device = nullptr;

    // The properties are sorted, i.e. there is a contiguous block for each device
    int id = -1;
    int lun = -1;
    bool is_active = false;
    for (const property_map &properties = property_handler.GetProperties(); const auto& [key, value] : properties) {
        if (!key.starts_with("device.")) {
            continue;
        }

        const auto &key_components = Split(key, '.', 3);
        if (key_components.size() < 3) {
            throw parser_exception(fmt::format("Invalid device definition '{}'", key));
        }

        const auto &id_and_lun = key_components[1];
        if (const string& error = SetIdAndLun(device_definition, id_and_lun);
            !error.empty()) {
            throw parser_exception(error);
        }

        // Check whether the device is active at the start of a new device block
        if (id != device_definition.id() || lun != device_definition.unit()) {
            is_active = CheckActive(properties, id_and_lun);
        }

        if (!is_active) {
            continue;
        }

        // Create a new device at the start of a new active device block
        if (id != device_definition.id() || lun != device_definition.unit()) {
            device = command.add_devices();
            id = device_definition.id();
            lun = device_definition.unit();
            device->set_id(id);
            device->set_unit(lun);
        }

        assert(device);
        SetDeviceProperties(*device, key_components[2], value);
    }

    AttachInitialDevices(command);
}

void S2p::AttachInitialDevices(PbCommand &command)
{
    if (command.devices_size()) {
        command.set_operation(ATTACH);

        CommandContext context(command);
        context.SetLocale(property_handler.GetProperty(PropertyHandler::LOCALE, GetLocale()));
        if (!executor->ProcessCmd(context)) {
            throw parser_exception("Can't attach devices");
        }

#ifdef BUILD_SCHS
        // Ensure that all host services have a dispatcher
        for (auto device : controller_factory.GetAllDevices()) {
            if (device->GetType() == SCHS) {
                static_pointer_cast<HostServices>(device)->SetDispatcher(dispatcher);
            }
        }
#endif
    }
}

bool S2p::CheckActive(const property_map &properties, const string &id_and_lun)
{
    if (const auto &it = properties.find("device." + id_and_lun + ".active"); it != properties.end()) {
        const string &active = it->second;
        if (active != "true" && active != "false") {
            throw parser_exception(fmt::format("Invalid boolean: '{}'", active));
        }
        return active == "true";
    }

    return true;
}

void S2p::SetDeviceProperties(PbDeviceDefinition &device, const string &key, const string &value)
{
    if (key == PropertyHandler::ACTIVE) {
        // "active" has already been handled separately
        return;
    }
    else if (key == PropertyHandler::TYPE) {
        device.set_type(ParseDeviceType(value));
    }
    else if (key == PropertyHandler::SCSI_LEVEL) {
        if (int scsi_level; !GetAsUnsignedInt(value, scsi_level) || !scsi_level) {
            throw parser_exception(fmt::format("Invalid SCSI level: '{}'", value));
        }
        else {
            device.set_scsi_level(scsi_level);
        }
    }
    else if (key == PropertyHandler::BLOCK_SIZE) {
        if (int block_size; !GetAsUnsignedInt(value, block_size)) {
            throw parser_exception(fmt::format("Invalid block size: '{}'", value));
        }
        else {
            device.set_block_size(block_size);
        }
    }
    else if (key == PropertyHandler::CACHING_MODE) {
        device.set_caching_mode(ParseCachingMode(value));
    }
    else if (key == PropertyHandler::NAME) {
        SetProductData(device, value);
    }
    else if (key == PropertyHandler::PARAMS) {
        ParseParameters(device, value);
    }
    else {
        SetParam(device, key, value);
    }
}

void S2p::ProcessScsiCommands()
{
    while (service_thread.IsRunning()) {
        // Only process the SCSI command if the bus is not busy and no other device responded
        if (bus->WaitForSelection() && WaitForNotBusy()) {
            scoped_lock<mutex> lock(executor->GetExecutionLocker());

            // Process command on the responsible controller based on the current initiator and target ID
            if (const auto shutdown_mode = controller_factory.ProcessOnController(bus->GetDAT()); shutdown_mode
                != shutdown_mode::none) {
                // When the bus is free SCSI2Pi or the Pi may be shut down.
                dispatcher->ShutDown(shutdown_mode);
            }
        }
    }
}

bool S2p::ExecuteCommand(CommandContext &context)
{
    if (const string &locale = GetParam(context.GetCommand(), "locale"); !locale.empty()) {
        context.SetLocale(locale);
    }

    if (!access_token.empty() && access_token != GetParam(context.GetCommand(), "token")) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_AUTHENTICATION, UNAUTHORIZED);
    }

    PbResult result;
    const bool status = dispatcher->DispatchCommand(context, result);
    if (status && context.GetCommand().operation() == PbOperation::SHUT_DOWN) {
        CleanUp();
        exit(EXIT_SUCCESS);
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
