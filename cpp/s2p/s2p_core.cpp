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
#include "base/property_handler.h"
#include "shared/s2p_util.h"
#include "shared/shared_exceptions.h"
#include "shared/s2p_version.h"
#include "shared_protobuf/protobuf_util.h"
#ifdef BUILD_SCHS
#include "devices/host_services.h"
#endif
#include "s2p_core.h"

using namespace std;
using namespace spdlog;
using namespace s2p_interface;
using namespace s2p_util;
using namespace protobuf_util;
using namespace scsi_defs;

void S2p::Banner(span<char*> args, bool usage) const
{
    if (usage) {
        cout << "\nUsage: " << args[0] << " [-id|hd ID[:LUN]] FILE] ...\n\n"
            << " id|ID is a SCSI device ID (0-" << (ControllerFactory::GetIdMax() - 1) << ").\n"
            << " hd|HD is a SASI device ID (0-" << (ControllerFactory::GetIdMax() - 1) << ").\n"
            << " LUN is the optional logical unit, 0 is the default"
            << " (SCSI: 0-" << (ControllerFactory::GetScsiLunMax() - 1) << ")"
            << ", SASI: 0-" << (ControllerFactory::GetSasiLunMax() - 1) << ").\n"
            << " Attaching a SASI drive (-hd instead of -id) selects SASI compatibility.\n"
            << " FILE is either a disk image file, \"daynaport\", \"printer\" or \"services\".\n"
            << " The image type is derived from the extension when no type is specified:\n"
            << "  hd1: SCSI HD image (Non-removable SCSI-1-CCS HD image)\n"
            << "  hds: SCSI HD image (Non-removable SCSI-2 HD image)\n"
            << "  hda: SCSI HD image (Apple compatible non-removable SCSI-2 HD image)\n"
            << "  hdr: SCSI HD image (Removable SCSI-2 HD image)\n"
            << "  mos: SCSI MO image (SCSI-2 MO image)\n"
            << "  iso: SCSI CD image (SCSI-2 ISO 9660 image)\n"
            << "  is1: SCSI CD image (SCSI-1-CCS ISO 9660 image)\n"
            << " Run 'man s2p' for other options.\n" << flush;

        exit(EXIT_SUCCESS);
    }
    else {
        cout << s2p_util::Banner("(Target Emulation)") << flush;
    }
}

bool S2p::InitBus(bool in_process)
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

string S2p::ParseArguments(span<char*> args, PbCommand &command, int &port, string &reserved_ids)
{
    string log_level = "info";
    PbDeviceType type = UNDEFINED;
    int block_size = 0;
    string name;
    string id_and_lun;

    string locale = GetLocale();

    // Avoid duplicate messages while parsing
    set_level(level::off);

    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-Ii-Hhb:d:n:p:r:t:z:C:D:F:L:P:R:v")) != -1) {
        switch (opt) {
        // The two option pairs below are kind of a compound option with two letters
        case 'i':
            case 'I':
            continue;

        case 'h':
            case 'H':
            is_sasi = true;
            continue;

        case 'd':
            case 'D':
            id_and_lun = optarg;
            continue;

        case 'b':
            if (!GetAsUnsignedInt(optarg, block_size)) {
                throw parser_exception("Invalid block size " + string(optarg));
            }
            continue;

        case 'z':
            locale = optarg;
            continue;

        case 'F':
            if (const string error = s2p_image.SetDefaultFolder(optarg); !error.empty()) {
                throw parser_exception(error);
            }
            continue;

        case 'L':
            log_level = optarg;
            continue;

        case 'R':
            int depth;
            if (!GetAsUnsignedInt(optarg, depth)) {
                throw parser_exception("Invalid image file scan depth " + string(optarg));
            }
            s2p_image.SetDepth(depth);
            continue;

        case 'n':
            name = optarg;
            continue;

        case 'p':
            if (!GetAsUnsignedInt(optarg, port) || port <= 0 || port > 65535) {
                throw parser_exception("Invalid port " + string(optarg) + ", port must be between 1 and 65535");
            }
            continue;

        case 'P':
            ReadAccessToken(optarg);
            continue;

        case 'r':
            reserved_ids = optarg;
            continue;

        case 't':
            type = ParseDeviceType(optarg);
            continue;

        case 1:
            // Encountered filename
            break;

        default:
            Banner(args, true);
            break;
        }

        if (optopt) {
            Banner(args, false);
            break;
        }

        // Set up the device data

        auto device = command.add_devices();

        if (!id_and_lun.empty()) {
            if (const string error = SetIdAndLun(ControllerFactory::GetIdMax(), ControllerFactory::GetLunMax(),
                *device, id_and_lun); !error.empty()) {
                throw parser_exception(error);
            }
        }

        device->set_type(type);
        device->set_block_size(block_size);

        ParseParameters(*device, optarg);

        SetProductData(*device, name);

        type = UNDEFINED;
        block_size = 0;
        name = "";
        id_and_lun = "";
    }

    if (!CommandDispatcher::SetLogLevel(log_level)) {
        throw parser_exception("Invalid log level '" + log_level + "'");
    }

    return locale;
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

bool S2p::ExecuteWithLock(const CommandContext &context)
{
    scoped_lock<mutex> lock(executor->GetExecutionLocker());
    return executor->ProcessCmd(context);
}

bool S2p::HandleDeviceListChange(const CommandContext &context, PbOperation operation) const
{
    // ATTACH and DETACH return the resulting device list
    if (operation == ATTACH || operation == DETACH) {
        // A command with an empty device list is required here in order to return data for all devices
        PbCommand command;
        PbResult result;
        response.GetDevicesInfo(executor->Get_allDevices(), result, command, s2p_image.GetDefaultFolder());
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

    Banner(args, false);

    PbCommand command;
    string locale;
    string reserved_ids;
    int port = DEFAULT_PORT;
    try {
        locale = ParseArguments(args, command, port, reserved_ids);
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    if (!InitBus(in_process)) {
        cerr << "Error: Can't initialize bus" << endl;

        return EXIT_FAILURE;
    }

    if (const string error = service_thread.Init([this](CommandContext &context) {
        return ExecuteCommand(context);
    }, port); !error.empty()) {
        cerr << "Error: " << error << endl;
        CleanUp();
        return EXIT_FAILURE;
    }

    if (const string error = executor->SetReservedIds(reserved_ids); !error.empty()) {
        cerr << "Error: " << error << endl;
        CleanUp();
        return EXIT_FAILURE;
    }

    if (command.devices_size()) {
        // Attach all specified devices
        command.set_operation(ATTACH);

        if (const CommandContext context(command, s2p_image.GetDefaultFolder(), locale); !executor->ProcessCmd(
            context)) {
            cerr << "Error: Can't attach devices" << endl;
            CleanUp();
            return EXIT_FAILURE;
        }

#ifdef BUILD_SCHS
        // Ensure that all host services have a dispatcher
        for (auto device : controller_factory->GetAllDevices()) {
            if (auto host_services = dynamic_pointer_cast<HostServices>(device); host_services != nullptr) {
                host_services->SetDispatcher(dispatcher);
            }
        }
#endif
    }

    PropertyHandler::Instance().Init("");

    // Display and log the device list
    PbServerInfo server_info;
    response.GetDevices(executor->Get_allDevices(), server_info, s2p_image.GetDefaultFolder());
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
