//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <thread>
#include <getopt.h>
#include "shared/s2p_util.h"
#include "s2p/s2p_core.h"
#include "s2pctl/s2pctl_core.h"
#include "s2pdump/s2pdump_core.h"
#include "s2pexec/s2pexec_core.h"
#include "s2pproto/s2pproto_core.h"

using namespace std;
using namespace s2p_util;

void usage()
{
    cout << "Usage: in_process_test [options]\n"
        << "  --client/-c       Client to run against s2p (s2pctl|s2pdump|s2pexec»s2pproto),\n"
        << "                    default is s2pctl.\n"
        << "  --client-args/-a  Arguments to run client with, optional for s2pctl.\n"
        << "  --s2p-args/-s     Arguments to run s2p with.\n"
        << "  --help/-h         Display this help.\n";
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
        { "s2p-args", required_argument, nullptr, 's' },
        { nullptr, 0, nullptr, 0 }
    };

    string client = "s2pexec";
    string t_args;
    string c_args;

    optind = 1;
    int opt;
    while ((opt = getopt_long(argc, argv, "-a:c:hs:", options.data(), nullptr)) != -1) {
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

        case 's':
            t_args = optarg;
            break;

        default:
            usage();
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (client != "s2pctl" && client != "s2pdump" && client != "s2pexec" && client != "s2pproto") {
        cerr << "Invalid in-process test client: '" << client
            << "', client must be s2pctl, s2pdump, s2pexec or s2pproto" << endl;
        exit(EXIT_FAILURE);
    }

    // s2pctl and s2pexec do not require arguments because they support an interactive mode
    if (client != "s2pctl" && client != "s2pexec" && c_args.empty()) {
        cerr << "Test client '" << client << "' requires arguments" << endl;
        exit(EXIT_FAILURE);
    }

    vector<char*> client_args;
    add_arg(client_args, client);
    for (const auto &arg : Split(c_args, ' ')) {
        add_arg(client_args, arg);
    }

    vector<char*> target_args;
    add_arg(target_args, "s2p");
    add_arg(target_args, "--port");
    add_arg(target_args, "6870");
    for (const auto &arg : Split(t_args, ' ')) {
        add_arg(target_args, arg);
    }

#if !defined __FreeBSD__ && !defined __APPLE__
    auto s2p_thread = jthread([&target_args]() {
#else
        auto s2p_thread = thread([&target_args]() {
#endif
        auto s2p = make_unique<S2p>();
        s2p->Run(target_args, true);
    });

    if (client == "s2pctl") {
        // Ensure that s2p is up
        sleep(1);

        add_arg(client_args, "--port");
        add_arg(client_args, "6870");
        auto s2pctl = make_unique<S2pCtl>();
        s2pctl->Run(client_args);
    }
    else if (client == "s2pdump") {
        auto s2pdump = make_unique<S2pDump>();
        s2pdump->Run(client_args, true);
    }
    else if (client == "s2pexec") {
        auto s2pexec = make_unique<S2pExec>();
        s2pexec->Run(client_args, true);
    }
    else if (client == "s2pproto") {
        auto s22proto = make_unique<S2pProto>();
        s22proto->Run(client_args, true);
    }
    else {
        assert(false);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
