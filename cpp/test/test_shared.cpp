//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include <iostream>
#include <unistd.h>
#include "mocks.h"
#include "base/device_factory.h"
#include "buses/bus_factory.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_version.h"

using namespace filesystem;
using namespace s2p_util;

pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> testing::CreateDevice(PbDeviceType type, int lun,
    const string &extension)
{
    const auto controller = make_shared<NiceMock<MockAbstractController>>(lun);
    const auto device = DeviceFactory::Instance().CreateDevice(type, lun, extension);
    device->Init( { });

    EXPECT_TRUE(controller->AddDevice(device));

    return {controller, device};
}

vector<int> testing::CreateCdb(scsi_command cmd, const string &hex)
{
    vector<int> cdb;
    cdb.emplace_back(static_cast<int>(cmd));
    ranges::transform(HexToBytes(hex), back_inserter(cdb), [](const byte b) {return static_cast<int>(b);});
    cdb.resize(BusFactory::Instance().GetCommandBytesCount(cmd));
    return cdb;
}

vector<uint8_t> testing::CreateParameters(const string &hex)
{
    vector<uint8_t> parameters;
    ranges::transform(HexToBytes(hex), back_inserter(parameters), [](const byte b) {return static_cast<uint8_t>(b);});
    return parameters;
}

string testing::TestShared::GetVersion()
{
    return fmt::format("{0:02}{1}{2}", s2p_major_version, s2p_minor_version, s2p_revision);
}

void testing::TestShared::Inquiry(PbDeviceType type, device_type t, scsi_level l, const string &ident,
    int additional_length, bool removable, const string &extension)
{
    const auto [controller, device] = CreateDevice(type, 0, extension);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::inquiry);
    const span<uint8_t> &buffer = controller->GetBuffer();
    EXPECT_EQ(t, static_cast<device_type>(buffer[0]));
    EXPECT_EQ(removable ? 0x80 : 0x00, buffer[1]);
    EXPECT_EQ(l, static_cast<scsi_level>(buffer[2]));
    EXPECT_EQ(l > scsi_level::scsi_2 ? scsi_level::scsi_2 : l, static_cast<scsi_level>(buffer[3]));
    EXPECT_EQ(additional_length, buffer[4]);
    string product_data;
    if (ident.size() == 24) {
        product_data = fmt::format("{0}{1:02}{2}{3}", ident, s2p_major_version, s2p_minor_version, s2p_revision);
    } else {
        product_data = ident;
    }
    EXPECT_EQ(product_data, string((const char* )(&buffer[8]), 28));
}

void testing::TestShared::TestRemovableDrive(PbDeviceType type, const string &filename, const string &product)
{
    const auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, filename);

    EXPECT_NE(nullptr, device);
    EXPECT_EQ(type, device->GetType());
    EXPECT_TRUE(device->SupportsFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_TRUE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_TRUE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_TRUE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("SCSI2Pi", device->GetVendor());
    EXPECT_EQ(product, device->GetProduct());
    EXPECT_EQ(GetVersion(), device->GetRevision());
}

void testing::TestShared::Dispatch(PrimaryDevice &device, scsi_command cmd, sense_key s, asc a, const string &msg)
{
    try {
        device.Dispatch(cmd);
        FAIL() << msg;
    }
    catch (const scsi_exception &e) {
        EXPECT_EQ(s, e.get_sense_key()) << msg;
        EXPECT_EQ(a, e.get_asc()) << msg;
    }
}

pair<int, path> testing::OpenTempFile(const string &extension)
{
    const string &filename = fmt::format("/tmp/scsi2pi_test-{}-XXXXXX", getpid()); // NOSONAR Publicly writable directory is fine here
    vector<char> f(filename.cbegin(), filename.cend());
    f.emplace_back(0);

    const int fd = mkstemp(f.data());
    EXPECT_NE(-1, fd) << "Couldn't create temporary file '" << f.data() << "'";

    path effective_name = f.data();
    if (!extension.empty()) {
        effective_name += "." + extension;
        rename(path(f.data()), effective_name);
    }

    TestShared::RememberTempFile(effective_name);

    return {fd, effective_name};
}

path testing::CreateTempFile(size_t size, const string &extension)
{
    return path(CreateTempFileWithData(vector<byte>(size), extension));
}

string testing::CreateTempFileWithData(const span<const byte> data, const string &extension)
{
    const auto& [fd, filename] = OpenTempFile(extension);

    const size_t count = write(fd, data.data(), data.size());
    close(fd);
    EXPECT_EQ(count, data.size()) << "Couldn't write to temporary file '" << filename << "'";

    return filename.string();
}

string testing::ReadTempFileToString(const string &filename)
{
    ifstream in(path(filename), ios::binary);
    stringstream buffer;
    buffer << in.rdbuf();

    return buffer.str();
}

void testing::SetUpProperties(string_view properties1, string_view properties2, const property_map &cmd_properties)
{
    string filenames;
    auto [fd1, filename1] = OpenTempFile();
    filenames = filename1;
    (void)write(fd1, properties1.data(), properties1.size());
    close(fd1);
    if (!properties2.empty()) {
        auto [fd2, filename2] = OpenTempFile();
        filenames += ",";
        filenames += filename2;
        (void)write(fd2, properties2.data(), properties2.size());
        close(fd2);
    }
    PropertyHandler::Instance().Init(filenames, cmd_properties, true);
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

uint32_t testing::GetInt16(const vector<uint8_t> &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 1);

    return (static_cast<uint32_t>(buf[offset]) << 8) | static_cast<uint32_t>(buf[offset + 1]);
}

uint32_t testing::GetInt32(const vector<uint8_t> &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    return (static_cast<uint32_t>(buf[offset]) << 24) | (static_cast<uint32_t>(buf[offset + 1]) << 16)
        | (static_cast<uint32_t>(buf[offset + 2]) << 8) | static_cast<uint32_t>(buf[offset + 3]);
}

uint64_t testing::GetInt64(const vector<uint8_t> &buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 7);

    return (static_cast<uint64_t>(buf[offset]) << 56) | (static_cast<uint64_t>(buf[offset + 1]) << 48) |
        (static_cast<uint64_t>(buf[offset + 2]) << 40) | (static_cast<uint64_t>(buf[offset + 3]) << 32) |
        (static_cast<uint64_t>(buf[offset + 4]) << 24) | (static_cast<uint64_t>(buf[offset + 5]) << 16) |
        (static_cast<uint64_t>(buf[offset + 6]) << 8) | static_cast<uint64_t>(buf[offset + 7]);
}
