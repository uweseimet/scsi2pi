//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <google/protobuf/message.h>
#include "s2p_core.h"

using namespace std;

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    vector<char*> args(argv, argv + argc);

    const int status = S2p().Run(args);

    google::protobuf::ShutdownProtobufLibrary();

    return status;
}
