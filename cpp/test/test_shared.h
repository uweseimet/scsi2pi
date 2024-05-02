//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <span>
#include "shared/s2p_util.h"
#include "shared/scsi.h"
#include "generated/s2p_interface.pb.h"

using namespace filesystem;
using namespace s2p_interface;

class PrimaryDevice;
class MockAbstractController;

namespace testing
{
pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> CreateDevice(PbDeviceType, int lun = 0,
    const string& = "");

vector<int> CreateCdb(scsi_command, const string& = "");
vector<uint8_t> CreateParameters(const string&);

pair<int, path> OpenTempFile();
path CreateTempFile(size_t);
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

    static void CleanUp()
    {
        for (const string &filename : temp_files) {
            remove(path(filename));
        }
    }

    static void RememberTempFile(const string &filename)
    {
        temp_files.insert(filename);
    }

    inline static unordered_set<string, s2p_util::StringHash, equal_to<>> temp_files;
};
}
