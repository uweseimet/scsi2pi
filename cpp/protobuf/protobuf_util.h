//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <span>

using namespace std;

namespace google::protobuf
{
class MessageLite;
}

namespace protobuf_util
{

void SerializeMessage(int, const google::protobuf::MessageLite&);
void DeserializeMessage(int, google::protobuf::MessageLite&);
bool ReadBytes(int, span<byte>);
void WriteBytes(int, span<const uint8_t>);

}
