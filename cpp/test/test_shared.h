//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <span>
#include "shared/scsi.h"
#include "devices/storage_device.h"
#include "shared/property_handler.h"
#include "generated/s2p_interface.pb.h"

using namespace filesystem;
using namespace s2p_interface;
using enum SenseKey;
using enum Asc;
using enum ScsiCommand;

class PrimaryDevice;
class MockAbstractController;

namespace s2p_test
{
pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> CreateDevice(PbDeviceType, int lun = 0,
    const string& = "");

vector<uint8_t> CreateCdb(ScsiCommand, const string& = "");
vector<uint8_t> CreateParameters(const string&);

string CreateImageFile(StorageDevice&, size_t = 4096, const string& = "");

string CreateTempName();
pair<int, path> OpenTempFile(const string& = "");
path CreateTempFile(size_t = 0, const string& = "");
string CreateTempFileWithData(span<const byte>, const string& = "");
string ReadTempFileToString(const string&);

void SetUpProperties(string_view, string_view = "", const property_map& = { });

void Dispatch(shared_ptr<PrimaryDevice>, ScsiCommand, SenseKey = NO_SENSE, Asc = NO_ADDITIONAL_SENSE_INFORMATION,
    const string& = "");

void RequestSense(shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>);

class TestShared
{

public:

    static string GetVersion();
    static void RequestSense(shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>);
    static void Inquiry(PbDeviceType, DeviceType, ScsiLevel, const string&, int, bool, const string& = "");
    static void TestRemovableDrive(PbDeviceType, const string&, const string&);
    static void Dispatch(shared_ptr<PrimaryDevice>, ScsiCommand, SenseKey = NO_SENSE, Asc =
        NO_ADDITIONAL_SENSE_INFORMATION, const string& = "");

    static void CleanUp()
    {
        lock_guard lock(temp_files_mutex);

        for (const string &filename : temp_files) {
            error_code error;
            remove(path(filename), error);
        }
    }

    static void RememberTempFile(const string &filename)
    {
        lock_guard lock(temp_files_mutex);

        temp_files.insert(filename);
    }

    inline static mutex temp_files_mutex;

    inline static unordered_set<string, s2p_util::StringHash, equal_to<>> temp_files;
};
}
