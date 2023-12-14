//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
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

pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> CreateDevice(PbDeviceType, const string& = "");

pair<int, path> OpenTempFile();
path CreateTempFile(int);
path CreateTempFileWithData(span<const byte>);

void DeleteTempFile(const string&);

string ReadTempFileToString(const string &filename);

int GetInt16(const vector<byte>&, int);
uint32_t GetInt32(const vector<byte>&, int);

class TestShared
{

public:

    static string GetVersion();
    static void Inquiry(PbDeviceType, scsi_defs::device_type, scsi_defs::scsi_level, const string&, int, bool,
        const string& = "");
    static void TestRemovableDrive(PbDeviceType, const string&, const string&);
};
}
