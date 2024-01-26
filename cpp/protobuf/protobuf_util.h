//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
// Helper methods for setting up/evaluating protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <vector>
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

namespace protobuf_util
{
static const char KEY_VALUE_SEPARATOR = '=';

string GetParam(const auto &item, const string &key)
{
    const auto &it = item.params().find(key);
    return it != item.params().end() ? it->second : "";
}

void SetParam(auto &item, const string &key, string_view value)
{
    if (!key.empty() && !value.empty()) {
        auto &map = *item.mutable_params();
        map[key] = value;
    }
}

void ParseParameters(PbDeviceDefinition&, const string&);
string SetCommandParams(PbCommand&, const string&);
string SetFromGenericParams(PbCommand&, const string&);
void SetProductData(PbDeviceDefinition&, const string&);
string SetIdAndLun(int, int, PbDeviceDefinition&, const string&);
string ListDevices(const vector<PbDevice>&);

void SerializeMessage(int, const google::protobuf::Message&);
void DeserializeMessage(int, google::protobuf::Message&);
size_t ReadBytes(int, span<byte>);
size_t WriteBytes(int, span<uint8_t>);
}
