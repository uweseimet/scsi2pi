//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/s2p_util.h"
#include "s2p/s2p_core.h"
#include "s2pdump/s2pdump_core.h"
#include "s2pexec/s2pexec_core.h"
#include <thread>

using namespace std;
using namespace s2p_util;

void add_arg(vector<char*> &args, const string &arg)
{
    args.emplace_back(strdup(arg.c_str()));
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cerr << "Error: Target and initiator arguments are mandatory" << endl;
        exit(EXIT_FAILURE);
    }

    string client = "s2pdump";
    string t_args;
    string i_args;

    optind = 1;
    int opt;
    while ((opt = getopt(argc, argv, "-i:c:t:")) != -1) {
        switch (opt) {
        case 'c':
            client = optarg;
            break;

        case 'i':
            i_args = optarg;
            break;

        case 't':
            t_args = optarg;
            break;

        default:
            cerr << "Parser error" << endl;
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (client != "s2pdump" && client != "s2pexec") {
        cerr << "Invalid test client: '" << client << "', client must be s2pdump or s2pexec" << endl;
        exit(EXIT_FAILURE);
    }

    vector<char*> initiator_args;
    add_arg(initiator_args, client);
    for (const auto &arg : Split(i_args, ' ')) {
        add_arg(initiator_args, arg);
    }

    vector<char*> target_args;
    add_arg(target_args, "s2p");
    for (const auto &arg : Split(t_args, ' ')) {
        add_arg(target_args, arg);
    }

#if !defined __FreeBSD__ && !defined __APPLE__
    auto target_thread = jthread([&target_args]() {
#else
        auto target_thread = thread([&target_args]() {
#endif
        auto s2p = make_unique<S2p>();
        s2p->Run(target_args, true);
    });

    // Give s2p time to initialize
    sleep(1);

    if (client == "s2pdump") {
        auto s2pdump = make_unique<S2pDump>();
        s2pdump->Run(initiator_args, true);
    }
    else {
        auto s2pexec = make_unique<S2pExec>();
        s2pexec->Run(initiator_args, true);
    }

    exit(EXIT_SUCCESS);
}
