//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <span>
#include <string>
#include <filesystem>
#include "base/primary_device.h"
#include "shared/scsi.h"
#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace filesystem;
using namespace s2p_interface;

class MockAbstractController;

namespace testing
{
extern const path test_data_temp_path;

pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> CreateDevice(PbDeviceType, int lun = 0,
    const string& = "");

vector<int> CreateCdb(scsi_command, const string&);
vector<uint8_t> CreateParameters(const string&);

pair<int, path> OpenTempFile();
path CreateTempFile(int);
path CreateTempFileWithData(span<const byte>);
string ReadTempFileToString(const string&);

int GetInt16(const vector<byte>&, int);
uint32_t GetInt32(const vector<byte>&, int);

class TestShared
{

public:

    static string GetVersion();
    static void Inquiry(PbDeviceType, device_type, scsi_level, const string&, int, bool, const string& = "");
    static void TestRemovableDrive(PbDeviceType, const string&, const string&);
    static void Dispatch(PrimaryDevice&, scsi_command, sense_key, asc, const string& = "");
};
}
