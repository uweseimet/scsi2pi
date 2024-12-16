//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <span>
#include "shared/scsi.h"
#include "base/property_handler.h"
#include "devices/storage_device.h"
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

string CreateImageFile(StorageDevice&, size_t = 4096, const string& = "");

string CreateTempName();
pair<int, path> OpenTempFile(const string& = "");
path CreateTempFile(size_t = 0, const string& = "");
string CreateTempFileWithData(span<const byte>, const string& = "");
string ReadTempFileToString(const string&);

void SetUpProperties(string_view, string_view = "", const property_map& = { });

void Dispatch(shared_ptr<PrimaryDevice>, scsi_command, sense_key = sense_key::no_sense, asc =
    asc::no_additional_sense_information, const string& = "");

void RequestSense(shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>);

class TestShared
{

public:

    static string GetVersion();
    static void RequestSense(shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>);
    static void Inquiry(PbDeviceType, device_type, scsi_level, const string&, int, bool, const string& = "");
    static void TestRemovableDrive(PbDeviceType, const string&, const string&);
    static void Dispatch(shared_ptr<PrimaryDevice>, scsi_command, sense_key = sense_key::no_sense, asc =
        asc::no_additional_sense_information, const string& = "");

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
