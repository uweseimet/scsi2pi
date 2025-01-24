//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <unordered_map>
#include <vector>
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

namespace protobuf_util
{

static constexpr char KEY_VALUE_SEPARATOR = '=';

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

PbDeviceType ParseDeviceType(const string&);
PbCachingMode ParseCachingMode(const string&);
void ParseParameters(PbDeviceDefinition&, const string&);
string SetCommandParams(PbCommand&, const string&);
string SetFromGenericParams(PbCommand&, const string&);
void SetProductData(PbDeviceDefinition&, const string&);
string SetIdAndLun(PbDeviceDefinition&, const string&);
int GetLunMax(PbDeviceType);

string ListDevices(const vector<PbDevice>&);

void SerializeMessage(int, const google::protobuf::Message&);
void DeserializeMessage(int, google::protobuf::Message&);
size_t ReadBytes(int, span<byte>);
size_t WriteBytes(int, span<uint8_t>);

inline static const unordered_map<int, PbDeviceType> DEVICE_TYPES = {
    { 'c', SCCD },
    { 'd', SCDP },
    { 'h', SCHD },
    { 'l', SCLP },
    { 'm', SCMO },
    { 'r', SCRM },
    { 's', SCHS },
    { 't', SCTP }
};
}
