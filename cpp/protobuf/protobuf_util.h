//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <google/protobuf/message.h>

using namespace std;

namespace protobuf_util
{

void SerializeMessage(int, const google::protobuf::Message&);
void DeserializeMessage(int, google::protobuf::Message&);
ssize_t ReadBytes(int, span<byte>);
ssize_t WriteBytes(int, span<const uint8_t>);

}
