//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2ptool.h"
#include <getopt.h>
#include "s2p/s2p_core.h"
#include "s2pctl/s2pctl_core.h"
#include "s2pdump/s2pdump_core.h"
#include "s2pexec/s2pexec_core.h"
#include "s2pproto/s2pproto_core.h"
#include "shared/s2p_util.h"

using namespace s2ptool;
using namespace s2p_util;

void s2ptool::Usage()
{
    cout << "SCSI Device Emulator and SCSI Tools SCSI2Pi (Virtual Bus Tool)\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2023-2026 Uwe Seimet\n";

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
    string s_args;
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
            Usage();
            return EXIT_SUCCESS;

        case 'l':
            log_signals = true;
            break;

        case 's':
            s_args = optarg;
            break;

        case 'v':
            cout << GetVersionString() << '\n';
            return EXIT_SUCCESS;

        default:
            Usage();

            return EXIT_FAILURE;
        }
    }

    const unordered_set<string> clients = { "s2pctl", "s2pdump", "s2pexec", "s2pproto" };
    if (!clients.contains(client)) {
        cerr << "Invalid virtual bus client: '" << client << "'\n";
        return EXIT_FAILURE;
    }

    vector<char*> client_args;
    AddArg(client_args, client);
    for (const auto &arg : Split(c_args, ' ')) {
        AddArg(client_args, arg != "''" && arg != "\"\"" ? arg : "");
    }

    vector<char*> s2p_args;
    AddArg(s2p_args, "s2p");
    for (const auto &arg : Split(s_args, ' ')) {
        AddArg(s2p_args, arg != "''" && arg != "\"\"" ? arg : "");
    }

    const auto s2p = make_shared<S2p>();
    auto s2p_thread = jthread([s2p, s2p_args, log_signals]() mutable {
        s2p->Run(s2p_args, true, log_signals);
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

    s2p_instance = s2p;

    SetTerminationHandler(TerminationHandler);

    if (client == "s2pctl") {
        runnable = make_unique<S2pCtl>();
    }
    else if (client == "s2pdump") {
        runnable = make_unique<S2pDump>();
    }
    else if (client == "s2pexec") {
        runnable = make_unique<S2pExec>();
    }
    else if (client == "s2pproto") {
        runnable = make_unique<S2pProto>();
    }
    const int result = runnable->Run(client_args, true, log_signals);

    TerminationHandler(result - 128);

    // Never reached
    return result;
}

void s2ptool::AddArg(vector<char*> &args, const string &arg)
{
    args.emplace_back(strdup(arg.c_str()));
}

void s2ptool::TerminationHandler(int sig)
{
    if (runnable) {
        runnable->CleanUp();
    }

    if (s2p_instance) {
        s2p_instance->CleanUp();
    }

    exit(sig + 128);
}
