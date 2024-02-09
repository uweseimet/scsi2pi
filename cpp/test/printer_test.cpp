//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "base/device_factory.h"
#include "devices/printer.h"

using namespace std;

TEST(PrinterTest, Device_Defaults)
{
    auto device = DeviceFactory::Instance().CreateDevice(UNDEFINED, 0, "printer");
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

TEST(PrinterTest, GetDefaultParams)
{
    const auto [controller, printer] = CreateDevice(SCLP);
    const auto params = printer->GetDefaultParams();
    EXPECT_EQ(1U, params.size());
}

TEST(PrinterTest, Init)
{
    auto [controller, printer] = CreateDevice(SCLP);
    EXPECT_TRUE(printer->Init( { }));

    param_map params;
    params["cmd"] = "missing_filename_specifier";
    EXPECT_FALSE(printer->Init(params));

    params["cmd"] = "%f";
    EXPECT_TRUE(printer->Init(params));
}

TEST(PrinterTest, TestUnitReady)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(PrinterTest, Inquiry)
{
    TestShared::Inquiry(SCLP, device_type::printer, scsi_level::scsi_2, "SCSI2Pi SCSI PRINTER    ", 0x1f, false);
}

TEST(PrinterTest, ReserveUnit)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_reserve6));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(PrinterTest, ReleaseUnit)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_release6));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(PrinterTest, SendDiagnostic)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_send_diagnostic));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(PrinterTest, Print)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, DataOut());
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_print));

    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    TestShared::Dispatch(*printer, scsi_command::cmd_print, sense_key::illegal_request,
        asc::invalid_field_in_cdb, "Buffer overflow was not reported");
}

TEST(PrinterTest, StopPrint)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_stop_print));
    EXPECT_EQ(status::good, controller->GetStatus());
}

TEST(PrinterTest, Synchronize_buffer)
{
    auto [controller, printer] = CreateDevice(SCLP);

    TestShared::Dispatch(*printer, scsi_command::cmd_synchronize_buffer, sense_key::aborted_command,
        asc::printer_nothing_to_print);

    // Further testing would use the printing system
}

TEST(PrinterTest, Write)
{
    auto [controller, printer] = CreateDevice(SCLP);

    const vector<uint8_t> buf(1);
    controller->SetTransferSize(1, 1);
    EXPECT_NO_THROW(dynamic_pointer_cast<Printer>(printer)->WriteData(buf, scsi_command::cmd_print));
}

TEST(PrinterTest, GetStatistics)
{
    Printer printer(0);

    EXPECT_EQ(4U, printer.GetStatistics().size());
}

