//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
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
size_t ReadBytes(int, span<byte>);
size_t WriteBytes(int, span<uint8_t>);

}
