//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>
#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "shared/s2p_version.h"

using namespace std;
using namespace filesystem;

// Include the process id in the temp file path so that multiple instances of the test procedures
// could run on the same host.
const path testing::test_data_temp_path(temp_directory_path() / // NOSONAR Publicly writable directory is fine here
    path(fmt::format("scsi2pi-test-{}", getpid())));

pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> testing::CreateDevice(PbDeviceType type, int lun,
    const string &extension)
{
    DeviceFactory device_factory;

    auto controller = make_shared<NiceMock<MockAbstractController>>(lun);
    auto device = device_factory.CreateDevice(type, lun, extension);
    device->Init( { });

    EXPECT_TRUE(controller->AddDevice(device));

    return {controller, device};
}

string testing::TestShared::GetVersion()
{
    return fmt::format("{0:02}{1:02}", s2p_major_version, s2p_minor_version);
}

void testing::TestShared::Inquiry(PbDeviceType type, device_type t, scsi_level l, const string &ident,
    int additional_length, bool removable, const string &extension)
{
    auto [controller, device] = CreateDevice(type, 0, extension);

    // ALLOCATION LENGTH
    controller->SetCmdByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::cmd_inquiry);
    const vector<uint8_t> &buffer = controller->GetBuffer();
    EXPECT_EQ(t, static_cast<device_type>(buffer[0]));
    EXPECT_EQ(removable ? 0x80 : 0x00, buffer[1]);
    EXPECT_EQ(l, static_cast<scsi_level>(buffer[2]));
    EXPECT_EQ(l > scsi_level::scsi_2 ? scsi_level::scsi_2 : l, static_cast<scsi_level>(buffer[3]));
    EXPECT_EQ(additional_length, buffer[4]);
    string product_data;
    if (ident.size() == 24) {
        product_data = fmt::format("{0}{1:02x}{2:02x}", ident, s2p_major_version, s2p_minor_version);
    } else {
        product_data = ident;
    }
    EXPECT_EQ(product_data, string((const char* )(&buffer[8]), 28));
}

void testing::TestShared::TestRemovableDrive(PbDeviceType type, const string &filename, const string &product)
{
    DeviceFactory device_factory;
    auto device = device_factory.CreateDevice(UNDEFINED, 0, filename);

    EXPECT_NE(nullptr, device);
    EXPECT_EQ(type, device->GetType());
    EXPECT_TRUE(device->SupportsFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_TRUE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_TRUE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_TRUE(device->IsLockable());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_TRUE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("SCSI2Pi", device->GetVendor());
    EXPECT_EQ(product, device->GetProduct());
    EXPECT_EQ(GetVersion(), device->GetRevision());
}

pair<int, path> testing::OpenTempFile()
{
    const string filename = string(test_data_temp_path) + "/scsi2pi_test-XXXXXX"; // NOSONAR Publicly writable directory is fine here
    vector<char> f(filename.begin(), filename.end());
    f.emplace_back(0);

    create_directories(path(filename).parent_path());

    const int fd = mkstemp(f.data());
    EXPECT_NE(-1, fd) << "Couldn't create temporary file '" << f.data() << "'";

    return make_pair(fd, path(f.data()));
}

path testing::CreateTempFile(int size)
{
    const auto data = vector<byte>(size);
    return CreateTempFileWithData(data);
}

path testing::CreateTempFileWithData(const span<const byte> data)
{
    const auto [fd, filename] = OpenTempFile();

    const size_t count = write(fd, data.data(), data.size());
    EXPECT_EQ(count, data.size()) << "Couldn't create temporary file '" << string(filename) << "'";
    close(fd);

    return path(filename);
}

void testing::DeleteTempFile(const string &filename)
{
    path temp_file = test_data_temp_path;
    temp_file += path(filename);
    remove(temp_file);
}

string testing::ReadTempFileToString(const string &filename)
{
    const path temp_file = test_data_temp_path / path(filename);
    ifstream in(temp_file);
    stringstream buffer;
    buffer << in.rdbuf();

    return buffer.str();
}

int testing::GetInt16(const vector<byte> &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    return (to_integer<int>(buf[offset]) << 8) | to_integer<int>(buf[offset + 1]);
}

uint32_t testing::GetInt32(const vector<byte> &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    return (to_integer<uint32_t>(buf[offset]) << 24) | (to_integer<uint32_t>(buf[offset + 1]) << 16)
        | (to_integer<uint32_t>(buf[offset + 2]) << 8) | to_integer<uint32_t>(buf[offset + 3]);
}
