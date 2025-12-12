//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2020-2023 Contributors to the PiSCSI project
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_core.h"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <netinet/in.h>
#include "base/device_factory.h"
#include "buses/bus_factory.h"
#include "command/command_context.h"
#include "command/command_dispatcher.h"
#include "command/command_image_support.h"
#include "command/command_response.h"
#ifdef BUILD_SCHS
#include "devices/host_services.h"
#endif
#include "protobuf/s2p_interface_util.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_version.h"
#include "s2p_parser.h"

using namespace s2p_interface_util;
using namespace s2p_parser;
using namespace s2p_util;

bool S2p::InitBus(bool in_process, bool log_signals)
{
    bus = bus_factory::CreateBus(true, in_process, APP_NAME, log_signals);
    if (!bus) {
        return false;
    }

    s2p_logger = CreateLogger(APP_NAME);

    executor = make_unique<CommandExecutor>(*bus, controller_factory, *s2p_logger);

    dispatcher = make_shared<CommandDispatcher>(*executor, controller_factory, *s2p_logger);

    return true;
}

void S2p::CleanUp()
{
    if (service_thread.IsRunning()) {
        service_thread.Stop();
    }

    PbCommand command;
    PbResult result;
    command.set_operation(DETACH_ALL);
    CommandContext context(command, *s2p_logger);
    dispatcher->DispatchCommand(context, result);

    // TODO Check why there are rare cases where bus is NULL on a remote interface shutdown
    // even though it is never set to NULL anywhere. This looks like a race condition.
    if (bus) {
        bus->CleanUp();
    }
}

void S2p::ReadAccessToken(const path &filename)
{
    if (error_code error; !is_regular_file(filename, error)) {
        throw ParserException("Access token file '" + filename.string() + "' must be a regular file");
    }

    if (struct stat st; stat(filename.c_str(), &st) || st.st_uid || st.st_gid) {
        throw ParserException("Access token file '" + filename.string() + "' must be owned by root");
    }

    if (const auto perms = filesystem::status(filename).permissions();
    (perms & perms::group_read) != perms::none || (perms & perms::others_read) != perms::none ||
        (perms & perms::group_write) != perms::none || (perms & perms::others_write) != perms::none) {
        throw ParserException("Access token file '" + filename.string() + "' must be readable by root only");
    }

    ifstream token_file(filename);
    if (!token_file) {
        throw ParserException("Can't open access token file '" + filename.string() + "'");
    }

    getline(token_file, access_token);
    if (token_file.fail()) {
        throw ParserException("Can't read access token file '" + filename.string() + "'");
    }

    if (access_token.empty()) {
        throw ParserException("Access token file '" + filename.string() + "' must not be empty");
    }
}

void S2p::LogDevices(const string &devices) const
{
    stringstream ss(devices);
    string line;

    while (getline(ss, line)) {
        s2p_logger->info(line);
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

    if (!InitBus(in_process, log_signals)) {
        cerr << "Error: Can't initialize bus\n";
        return EXIT_FAILURE;
    }

    Banner(false);

    bool ignore_conf = false;

    property_map properties;
    try {
        properties = ParseArguments(args, ignore_conf);
    }
    catch (const ParserException &e) {
        cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    int port;
    if (!ParseProperties(properties, port, ignore_conf)) {
        return EXIT_FAILURE;
    }

    if (const string &error = MapExtensions(); !error.empty()) {
        cerr << "Error: " << error << '\n';
        return EXIT_FAILURE;
    }

    controller_factory.SetFormatLimit(128);
    if (const string &log_limit = property_handler.RemoveProperty(PropertyHandler::LOG_LIMIT); !log_limit.empty()) {
        if (const int limit = ParseAsUnsignedInt(log_limit); limit < 0) {
            cerr << "Error: Invalid log limit '" << log_limit << "'\n";
            return EXIT_FAILURE;
        }
        else {
            controller_factory.SetFormatLimit(limit);
        }
    }

    if (const string &reserved_ids = property_handler.RemoveProperty(PropertyHandler::RESERVED_IDS); !reserved_ids.empty()) {
        if (const string &error = executor->SetReservedIds(reserved_ids); !error.empty()) {
            cerr << "Error: " << error << '\n';
            CleanUp();
            return EXIT_FAILURE;
        }
    }

    if (const string &token_file = property_handler.RemoveProperty(PropertyHandler::TOKEN_FILE); !token_file.empty()) {
        ReadAccessToken(path(token_file));
    }

    if (const string &error = service_thread.Init(port, [this](CommandContext &context) {
        return ExecuteCommand(context);
    }, s2p_logger); !error.empty()) {
        cerr << "Error: " << error << '\n';
        CleanUp();
        return EXIT_FAILURE;
    }

    try {
        CreateDevices();
    }
    catch (const ParserException &e) {
        cerr << "Error: " << e.what() << '\n';
        CleanUp();
        return EXIT_FAILURE;
    }

    for (const auto& [key, value] : property_handler.GetUnknownProperties()) {
        if (!key.starts_with(PropertyHandler::DEVICE)) {
            cerr << "Error: Invalid global property \"" << key << "\", check your command line and "
                << PropertyHandler::CONFIGURATION << '\n';
            CleanUp();
            return EXIT_FAILURE;
        }
    }

    // Display and log the device list
    PbServerInfo server_info;
    command_response::GetDevices(controller_factory.GetAllDevices(), server_info);
    const vector<PbDevice> &devices = { server_info.devices_info().devices().cbegin(),
        server_info.devices_info().devices().cend() };
    const string device_list = ListDevices(devices);
    LogDevices(device_list);

    // Show the device list only once, either the console or the log
    if (get_level() > level::info) {
        cout << device_list << flush;
    }

    if (!in_process && !bus->IsRaspberryPi()) {
        cout << "No RaSCSI/PiSCSI board support available, functionality is limited\n" << flush;
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

        if (const string &log_pattern = property_handler.RemoveProperty(PropertyHandler::LOG_PATTERN); !log_pattern.empty()) {
            s2p_logger->set_pattern(log_pattern);
            spdlog::set_pattern(log_pattern);
            controller_factory.SetLogPattern(log_pattern);
        }

        // This sets the global level only, there are no attached devices yet
        log_level = property_handler.RemoveProperty(PropertyHandler::LOG_LEVEL, "info");
        if (!dispatcher->SetLogLevel(log_level)) {
            throw ParserException("Invalid log level: '" + log_level + "'");
        }

        // Log the properties (on trace level) *after* the log level has been set
        LogProperties();

        if (const string &image_folder = property_handler.RemoveProperty(PropertyHandler::IMAGE_FOLDER); !image_folder.empty()) {
            if (const string &error = CommandImageSupport::GetInstance().SetDefaultFolder(image_folder); !error.empty()) {
                throw ParserException(error);
            }
            else {
                s2p_logger->info("Default image folder set to '{}'", image_folder);
            }
        }

        if (const string &scan_depth = property_handler.RemoveProperty(PropertyHandler::SCAN_DEPTH, "1"); !scan_depth.empty()) {
            if (const int depth = ParseAsUnsignedInt(scan_depth); depth < 0) {
                throw ParserException("Invalid image file scan depth: " + scan_depth);
            }
            else {
                CommandImageSupport::GetInstance().SetDepth(depth);
            }
        }

        if (const string &script_file = property_handler.RemoveProperty(PropertyHandler::SCRIPT_FILE); !script_file.empty()) {
            if (!controller_factory.SetScriptFile(script_file)) {
                throw ParserException("Can't create script file '" + script_file + "': " + strerror(errno));
            }
            s2p_logger->info("Generating script file '" + script_file + "'");
        }

        if (const string &without_types = property_handler.RemoveProperty(PropertyHandler::WITHOUT_TYPES); !dispatcher->SetWithoutTypes(
            without_types)) {
            throw ParserException("Invalid device types list: '" + without_types + "'");
        }

        const string &p = property_handler.RemoveProperty(PropertyHandler::PORT, "6868");
        port = ParseAsUnsignedInt(p);
        if (port <= 0 || port > 65535) {
            throw ParserException("Invalid port: '" + p + "', port must be between 1 and 65535");
        }
    }
    catch (const ParserException &e) {
        cerr << "Error: " << e.what() << '\n';
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
        property_handler.RemoveProperty(key);

        const auto &components = Split(key, '.');
        if (components.size() != 2) {
            return "Invalid extension mapping: '" + key + "'";
        }

        PbDeviceType type = UNDEFINED;
        if (PbDeviceType_Parse(ToUpper(components[1]), &type) && type == UNDEFINED) {
            continue;
        }

        for (const string &extension : Split(value, ',')) {
            if (!DeviceFactory::GetInstance().AddExtensionMapping(extension, type)) {
                return "Duplicate extension mapping for extension '" + extension + "'";
            }
        }
    }

    return "";
}

void S2p::LogProperties() const
{
    s2p_logger->trace("Effective startup properties:");
    for (const auto& [k, v] : property_handler.GetProperties()) {
        s2p_logger->trace("  {0}={1}", k, v);
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
    for (const property_map &properties = property_handler.GetProperties(PropertyHandler::DEVICE);
        const auto& [key, value] : properties) {
        const auto &key_components = Split(key, '.', 3);
        if (key_components.size() < 3) {
            throw ParserException(fmt::format("Invalid device definition '{}'", key));
        }

        const auto &id_and_lun = key_components[1];
        if (const string& error = SetIdAndLun(device_definition, id_and_lun);
            !error.empty()) {
            throw ParserException(error);
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

        CommandContext context(command, *s2p_logger);
        context.SetLocale(property_handler.RemoveProperty(PropertyHandler::LOCALE, GetLocale()));
        if (!executor->ProcessCmd(context)) {
            throw ParserException("Can't attach devices");
        }

#ifdef BUILD_SCHS
        // Ensure that all host services have a dispatcher
        for (auto device : controller_factory.GetAllDevices()) {
            if (auto services = dynamic_pointer_cast<HostServices>(device); services) {
                services->SetDispatcher(dispatcher);
            }
        }
#endif
    }
}

bool S2p::CheckActive(const property_map &properties, const string &id_and_lun)
{
    if (const auto &it = properties.find(PropertyHandler::DEVICE + id_and_lun + ".active"); it != properties.end()) {
        const string &active = it->second;
        if (active != "true" && active != "false") {
            throw ParserException(fmt::format("Invalid boolean: '{}'", active));
        }
        return active == "true";
    }

    return true;
}

void S2p::SetDeviceProperties(PbDeviceDefinition &device, const string &key, const string &value) const
{
    if (key == PropertyHandler::ACTIVE) {
        // "active" has already been handled separately
    }
    else if (key == PropertyHandler::TYPE) {
        device.set_type(ParseDeviceType(value));
    }
    else if (key == PropertyHandler::SCSI_LEVEL) {
        if (const int level = ParseAsUnsignedInt(value); level <= 0 || level >= static_cast<int>(ScsiLevel::LAST)) {
            throw ParserException(fmt::format("Invalid SCSI level: '{}'", value));
        }
        else {
            device.set_scsi_level(level);
        }
    }
    else if (key == PropertyHandler::BLOCK_SIZE) {
        if (const int block_size = ParseAsUnsignedInt(value); block_size < 0) {
            throw ParserException(fmt::format("Invalid block size: '{}'", value));
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
                != ShutdownMode::NONE) {
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
