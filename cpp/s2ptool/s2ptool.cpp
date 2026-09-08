//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2ptool.h"
#include <functional>
#include <unordered_map>
#include <getopt.h>
#include "buses/bus_factory.h"
#include "s2p/s2p_core.h"
#include "s2pctl/s2pctl_core.h"
#include "s2pdump/s2pdump_core.h"
#include "s2pexec/s2pexec_core.h"
#include "s2pproto/s2pproto_core.h"
#include "shared/runnable.h"
#include "shared/s2p_util.h"

using namespace s2ptool;
using namespace s2p_util;

void s2ptool::Usage()
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Virtual Bus Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2023-2026 Uwe Seimet\n";

    cout << "Usage: s2ptool [options]\n"
        << "  --client/-c COMMAND ARGS  The client tool to run against s2p (s2pctl|s2pdump|\n"
        << "                            s2pexec|s2pproto, including its command-line\n"
        << "                            arguments. Default tool is s2pexec.\n"
        << "  --help/-h                 Display this help.\n"
        << "  --log-signals/-l          On log level 'trace' log bus signals (verbose).\n"
        << "  --s2p-args/-s             Arguments to run s2p with.\n"
        << "  --version/-v              Display the s2ptool version.\n";
}

int main(int argc, char *argv[])
{
    const vector<option> options = {
        { "client", required_argument, nullptr, 'c' },
        { "help", no_argument, nullptr, 'h' },
        { "log-signals", no_argument, nullptr, 'l' },
        { "s2p-args", required_argument, nullptr, 's' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string command = "s2pexec";
    string args;
    bool log_signals = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(argc, argv, "-c:hls:v", options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'c':
            command = optarg;
            break;

        case 'h':
            Usage();
            return EXIT_SUCCESS;

        case 'l':
            log_signals = true;
            break;

        case 's':
            args = optarg;
            break;

        case 'v':
            cout << GetVersionString() << '\n';
            return EXIT_SUCCESS;

        default:
            Usage();
            return EXIT_FAILURE;
        }
    }

    vector<char*> client_args;
    for (const auto &arg : Split(command, ' ')) {
        if (client_args.empty() && arg.starts_with('-')) {
            AddArg(client_args, "s2pexec");
        }

        AddArg(client_args, arg != "''" && arg != "\"\"" ? arg : "");
    }

    unordered_map<string, function<unique_ptr<Runnable>()>> clients = {
        { "s2pctl", []()
            {
                return make_unique<S2pCtl>();
            } },
        { "s2pdump", []()
            {
                return make_unique<S2pDump>();
            } },
        { "s2pexec", []()
            {
                return make_unique<S2pExec>();
            } },
        { "s2pproto", []()
            {
                return make_unique<S2pProto>();
            } }
    };
    if (!clients.contains(client_args[0])) {
        cerr << "Invalid virtual bus client: '" << client_args[0] << "'\n";
        return EXIT_FAILURE;
    }

    vector<char*> s2p_args;
    AddArg(s2p_args, "s2p");
    for (const auto &arg : Split(args, ' ')) {
        AddArg(s2p_args, arg != "''" && arg != "\"\"" ? arg : "");
    }

    BusFactory::GetInstance().EnableVirtualBus(log_signals);

    const auto s2p = make_shared<S2p>();
    auto s2p_thread = jthread([s2p, s2p_args]() mutable {
        s2p->Run(s2p_args);
    });

    // Wait for s2p on the virtual bus up to 1 s
    const auto now = chrono::steady_clock::now();
    while (!s2p->Ready()) {
        if (chrono::steady_clock::now() - now >= chrono::seconds(1)) {
            break;
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    if (!s2p->Ready()) {
        cerr << "Error starting s2p on virtual bus\n";
        return EXIT_FAILURE;
    }

    const int result = clients[client_args[0]]()->Run(client_args);

    s2p->CleanUp();

    return result;
}

void s2ptool::AddArg(vector<char*> &args, const string &arg)
{
    args.emplace_back(strdup(arg.c_str()));
}
