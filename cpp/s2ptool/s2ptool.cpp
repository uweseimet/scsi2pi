//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <getopt.h>
#include "s2p/s2p_core.h"
#include "s2pctl/s2pctl_core.h"
#include "s2pdump/s2pdump_core.h"
#include "s2pexec/s2pexec_core.h"
#include "s2pproto/s2pproto_core.h"
#include "shared/s2p_util.h"

using namespace s2p_util;

void usage()
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (In-process Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2023-2025 Uwe Seimet\n";

    cout << "Usage: s2ptool [options]\n"
        << "  --client/-c CLIENT  The client tool to run against s2p (s2pctl|s2pdump|\n"
        << "                      s2pexec|s2pproto), default is s2pexec.\n"
        << "  --client-args/-a    Arguments to run the client tool with,\n"
        << "                      optional for s2pctl and s2pexec.\n"
        << "  --help/-h           Display this help.\n"
        << "  --log-signals/-l    On log level 'trace' also log bus signals.\n"
        << "  --s2p-args/-s       Arguments to run s2p with.\n"
        << "  --version/-v        Display the s2ptool version.\n";
}

void add_arg(vector<char*> &args, const string &arg)
{
    args.emplace_back(strdup(arg.c_str()));
}

int main(int argc, char *argv[])
{
    const vector<option> options = {
        { "client", required_argument, nullptr, 'c' },
        { "client-args", required_argument, nullptr, 'a' },
        { "help", no_argument, nullptr, 'h' },
        { "log-signals", no_argument, nullptr, 'l' },
        { "s2p-args", required_argument, nullptr, 's' },
        { "version", no_argument, nullptr, 'v' },
        { nullptr, 0, nullptr, 0 }
    };

    string client = "s2pexec";
    string t_args;
    string c_args;
    bool log_signals = false;

    optind = 1;
    int opt;
    while ((opt = getopt_long(argc, argv, "-a:c:hls:v", options.data(), nullptr)) != -1) {
        switch (opt) {
        case 'a':
            c_args = optarg;
            break;

        case 'c':
            client = optarg;
            break;

        case 'h':
            usage();
            exit(EXIT_SUCCESS);
            break;

        case 'l':
            log_signals = true;
            break;

        case 's':
            t_args = optarg;
            break;

        case 'v':
            cout << GetVersionString() << '\n';
            exit(EXIT_SUCCESS);
            break;

        default:
            usage();
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (client != "s2pctl" && client != "s2pdump" && client != "s2pexec" && client != "s2pproto") {
        cerr << "Invalid in-process test tool client: '" << client
            << "', client must be s2pctl, s2pdump, s2pexec or s2pproto\n";
        exit(EXIT_FAILURE);
    }

    // s2pctl and s2pexec do not require arguments because they support an interactive mode
    if (client != "s2pctl" && client != "s2pexec" && c_args.empty()) {
        cerr << "Test client '" << client << "' requires arguments\n";
        exit(EXIT_FAILURE);
    }

    vector<char*> client_args;
    add_arg(client_args, client);
    for (const auto &arg : Split(c_args, ' ')) {
        add_arg(client_args, arg != "''" && arg != "\"\"" ? arg : "");
    }

    vector<char*> target_args;
    add_arg(target_args, "s2p");
    for (const auto &arg : Split(t_args, ' ')) {
        add_arg(target_args, arg != "''" && arg != "\"\"" ? arg : "");
    }

    const auto s2p = make_shared<S2p>();
    auto s2p_thread = jthread([&target_args, log_signals, s2p]() {
        s2p->Run(target_args, true, log_signals);
    });

    // Wait for the in-process bus target up to 1 s
    const auto now = chrono::steady_clock::now();
    do {
        if (s2p->Ready()) {
            break;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 1);

    if (!s2p->Ready()) {
        cerr << "Error starting in-process s2p\n";
        exit(EXIT_FAILURE);
    }

    if (client == "s2pctl") {
        auto s2pctl = make_unique<S2pCtl>();
        s2pctl->Run(client_args);
    }
    else if (client == "s2pdump") {
        auto s2pdump = make_unique<S2pDump>();
        s2pdump->Run(client_args, true, log_signals);
    }
    else if (client == "s2pexec") {
        auto s2pexec = make_unique<S2pExec>();
        s2pexec->Run(client_args, true, log_signals);
    }
    else if (client == "s2pproto") {
        auto s22proto = make_unique<S2pProto>();
        s22proto->Run(client_args, true, log_signals);
    }
    else {
        assert(false);
        exit(EXIT_FAILURE);
    }

    s2p->CleanUp();

    exit(EXIT_SUCCESS);
}
