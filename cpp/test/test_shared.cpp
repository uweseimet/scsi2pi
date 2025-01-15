//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fstream>
#include <iostream>
#include <unistd.h>
#include "mocks.h"
#include "base/device_factory.h"
#include "shared/command_meta_data.h"
#include "shared/s2p_exceptions.h"
#include "shared/s2p_version.h"

using namespace filesystem;
using namespace s2p_util;

pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> testing::CreateDevice(PbDeviceType type, int lun,
    const string &extension)
{
    const auto controller = make_shared<NiceMock<MockAbstractController>>(lun);
    const auto device = DeviceFactory::Instance().CreateDevice(type, lun, extension);
    device->SetParams( { });
    device->Init();

    EXPECT_TRUE(controller->AddDevice(device));

    return {controller, device};
}

vector<int> testing::CreateCdb(ScsiCommand cmd, const string &hex)
{
    vector<int> cdb;
    cdb.emplace_back(static_cast<int>(cmd));
    ranges::transform(HexToBytes(hex), back_inserter(cdb), [](const byte b) {return static_cast<int>(b);});
    if (CommandMetaData::Instance().GetByteCount(cmd)) {
        cdb.resize(CommandMetaData::Instance().GetByteCount(cmd));
    }
    return cdb;
}

vector<uint8_t> testing::CreateParameters(const string &hex)
{
    vector<uint8_t> parameters;
    ranges::transform(HexToBytes(hex), back_inserter(parameters), [](const byte b) {return static_cast<uint8_t>(b);});
    return parameters;
}

string testing::CreateImageFile(StorageDevice &device, size_t size, const string &extension)
{
    const auto &filename = CreateTempFile(size, extension);
    device.SetFilename(filename.string());
    device.Open();
    return filename.string();
}

string testing::TestShared::GetVersion()
{
    return fmt::format("{0:02}{1}{2}", s2p_major_version, s2p_minor_version, s2p_revision);
}

void testing::TestShared::RequestSense(shared_ptr<MockAbstractController> controller, shared_ptr<PrimaryDevice> device)
{
    // Allocation length
    controller->SetCdbByte(4, 255);
    Dispatch(device, ScsiCommand::REQUEST_SENSE);
}

void testing::TestShared::Inquiry(PbDeviceType type, DeviceType t, ScsiLevel l, const string &ident,
    int additional_length, bool removable, const string &extension)
{
    const auto [controller, device] = CreateDevice(type, 0, extension);

    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn);
    device->Dispatch(ScsiCommand::INQUIRY);
    const auto &buffer = controller->GetBuffer();
    EXPECT_EQ(t, static_cast<DeviceType>(buffer[0]));
    EXPECT_EQ(removable ? 0x80 : 0x00, buffer[1]);
    EXPECT_EQ(l, static_cast<ScsiLevel>(buffer[2]));
    EXPECT_EQ(l > ScsiLevel::SCSI_2 ? ScsiLevel::SCSI_2 : l, static_cast<ScsiLevel>(buffer[3]));
    EXPECT_EQ(additional_length, buffer[4]);
    string product_data;
    if (ident.size() == 24) {
        product_data = fmt::format("{0}{1:02}{2}{3}", ident, s2p_major_version, s2p_minor_version, s2p_revision);
    } else {
        product_data = ident;
    }
    EXPECT_EQ(product_data, string((const char* )buffer.data() + 8, 28));
}

void testing::TestShared::TestRemovableDrive(PbDeviceType type, const string &filename, const string &product)
{
    const auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, filename);

    EXPECT_NE(nullptr, device);
    EXPECT_EQ(type, device->GetType());
    EXPECT_TRUE(device->SupportsImageFile());
    EXPECT_FALSE(device->SupportsParams());
    EXPECT_TRUE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_TRUE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_TRUE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    const auto& [v, p, r] = device->GetProductData();
    EXPECT_EQ("SCSI2Pi", v);
    EXPECT_EQ(product, p);
    EXPECT_EQ(GetVersion(), r);
}

void testing::TestShared::Dispatch(shared_ptr<PrimaryDevice> device, ScsiCommand cmd, SenseKey sense_key, Asc asc,
    const string &msg)
{
    try {
        device->Dispatch(cmd);
        if (sense_key != SenseKey::NO_SENSE || asc != Asc::NO_ADDITIONAL_SENSE_INFORMATION) {
            FAIL() << msg;
        }
    }
    catch (const ScsiException &e) {
        if (e.get_sense_key() != sense_key || e.get_asc() != asc) {
            spdlog::critical("Expected: " + FormatSenseData(sense_key, asc));
            spdlog::critical("Actual: " + FormatSenseData(e.get_sense_key(), e.get_asc()));
            FAIL() << msg;
        }
    }

    auto *controller = dynamic_cast<MockAbstractController*>(device->GetController());
    if (controller) {
        controller->ResetCdb();
    }
}

string testing::CreateTempName()
{
    error_code error;
    return fmt::format("{}/scsi2pi_test-XXXXXX", temp_directory_path(error).string()); // NOSONAR Publicly writable directory is safe here
}

pair<int, path> testing::OpenTempFile(const string &extension)
{
    char *f = strdup(CreateTempName().c_str());
    const int fd = mkstemp(f);
    const path filename = f;
    free(f); // NOSONAR Required because of mkstemp
    EXPECT_NE(-1, fd) << "Couldn't create temporary file '" << filename << "'";

    path effective_name = filename;
    if (!extension.empty()) {
        effective_name += "." + extension;
        rename(path(filename), effective_name);
    }

    TestShared::RememberTempFile(effective_name);

    return {fd, effective_name};
}

path testing::CreateTempFile(size_t size, const string &extension)
{
    return path(CreateTempFileWithData(vector<byte>(size), extension));
}

string testing::CreateTempFileWithData(span<const byte> data, const string &extension)
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

void testing::RequestSense(shared_ptr<MockAbstractController> controller, shared_ptr<PrimaryDevice> device)
{
    TestShared::RequestSense(controller, device);
}

void testing::Dispatch(shared_ptr<PrimaryDevice> device, ScsiCommand command, SenseKey s, Asc a, const string &msg)
{
    TestShared::Dispatch(device, command, s, a, msg);
}
