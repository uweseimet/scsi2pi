//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/s2p_util.h"
#include "s2p/s2p_core.h"
#include "s2pdump/s2pdump_core.h"
#include <thread>

using namespace std;
using namespace s2p_util;

void add_arg(vector<char*> &args, const string &arg)
{
    args.push_back(strdup(arg.c_str()));
}

int main(int argc, char *argv[])
{
    string t_args;
    string i_args;

    int opt;
    while ((opt = getopt(argc, argv, "-i:t:")) != -1) {
        switch (opt) {
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

    vector<char*> initiator_args;
    add_arg(initiator_args, "initiator");
    for (const auto &arg : Split(i_args, ' ')) {
        add_arg(initiator_args, arg);
    }

    vector<char*> target_args;
    add_arg(target_args, "target");
    for (const auto &arg : Split(t_args, ' ')) {
        add_arg(target_args, arg);
    }

#if !defined __FreeBSD__ && !defined __APPLE__
    auto target_thread = jthread([&target_args]() {
#else
        auto target_thread = thread([&target_args]() {
#endif
        auto s2p = make_unique<S2p>();
        s2p->run(target_args, true);
    });

    // TODO Avoid sleeping
    sleep(1);

    auto s2pdump = make_unique<S2pDump>();
    s2pdump->run(initiator_args, true);

    exit(EXIT_SUCCESS);
}
