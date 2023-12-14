//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "devices/printer.h"

using namespace std;

TEST(ScsiPrinterTest, Device_Defaults)
{
    DeviceFactory device_factory;

    auto device = device_factory.CreateDevice(UNDEFINED, 0, "printer");
    EXPECT_NE(nullptr, device);
    EXPECT_EQ(SCLP, device->GetType());
    EXPECT_FALSE(device->SupportsFile());
    EXPECT_TRUE(device->SupportsParams());
    EXPECT_FALSE(device->IsProtectable());
    EXPECT_FALSE(device->IsProtected());
    EXPECT_FALSE(device->IsReadOnly());
    EXPECT_FALSE(device->IsRemovable());
    EXPECT_FALSE(device->IsRemoved());
    EXPECT_FALSE(device->IsLockable());
    EXPECT_FALSE(device->IsLocked());
    EXPECT_FALSE(device->IsStoppable());
    EXPECT_FALSE(device->IsStopped());

    EXPECT_EQ("SCSI2Pi", device->GetVendor());
    EXPECT_EQ("SCSI PRINTER", device->GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), device->GetRevision());
}

TEST(ScsiPrinterTest, GetDefaultParams)
{
    const auto [controller, printer] = CreateDevice(SCLP);
    const auto params = printer->GetDefaultParams();
    EXPECT_EQ(1, params.size());
}

TEST(ScsiPrinterTest, Init)
{
    auto [controller, printer] = CreateDevice(SCLP);
    EXPECT_TRUE(printer->Init( { }));

    param_map params;
    params["cmd"] = "missing_filename_specifier";
    EXPECT_FALSE(printer->Init(params));

    params["cmd"] = "%f";
    EXPECT_TRUE(printer->Init(params));
}

TEST(ScsiPrinterTest, TestUnitReady)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    printer->Dispatch(scsi_command::cmd_test_unit_ready);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, Inquiry)
{
    TestShared::Inquiry(SCLP, device_type::printer, scsi_level::scsi_2, "SCSI2Pi SCSI PRINTER    ", 0x1f, false);
}

TEST(ScsiPrinterTest, ReserveUnit)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::cmd_reserve6);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, ReleaseUnit)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::cmd_release6);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, SendDiagnostic)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    printer->Dispatch(scsi_command::cmd_send_diagnostic);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, Print)
{
    auto [controller, printer] = CreateDevice(SCLP);
    // Required by the bullseye clang++ compiler
    auto p = printer;

    EXPECT_CALL(*controller, DataOut());
    printer->Dispatch(scsi_command::cmd_print);

    controller->SetCmdByte(3, 0xff);
    controller->SetCmdByte(4, 0xff);
    EXPECT_THAT([&] {p->Dispatch(scsi_command::cmd_print);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb)))) << "Buffer overflow was not reported";
}

TEST(ScsiPrinterTest, StopPrint)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    printer->Dispatch(scsi_command::cmd_stop_print);
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(ScsiPrinterTest, Synchronize_buffer)
{
    auto [controller, printer] = CreateDevice(SCLP);
    // Required by the bullseye clang++ compiler
    auto p = printer;

    EXPECT_THAT([&] {p->Dispatch(scsi_command::cmd_synchronize_buffer);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
                Property(&scsi_exception::get_asc, asc::no_additional_sense_information)))) << "Nothing to print";

    // Further testing would use the printing system
}

TEST(ScsiPrinterTest, WriteByteSequence)
{
    auto [controller, printer] = CreateDevice(SCLP);

    const vector<uint8_t> buf(1);
    EXPECT_TRUE(printer->WriteByteSequence(buf));
}
