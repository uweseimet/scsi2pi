//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <google/protobuf/message.h>
#include "s2pproto_core.h"

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    vector<char*> args(argv, argv + argc);

    const int status = S2pProto().Run(args, false);

    google::protobuf::ShutdownProtobufLibrary();

    return status;
}
