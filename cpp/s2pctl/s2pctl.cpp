//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_core.h"

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    const vector<char*> args(argv, argv + argc);

    const int status = S2pCtl().Run(args);

    google::protobuf::ShutdownProtobufLibrary();

    return status;
}
