//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/printer.h"
#include "shared/s2p_exceptions.h"

TEST(PrinterTest, Device_Defaults)
{
    Printer printer(0);

    EXPECT_EQ(SCLP, printer.GetType());
    EXPECT_FALSE(printer.SupportsFile());
    EXPECT_TRUE(printer.SupportsParams());
    EXPECT_FALSE(printer.IsProtectable());
    EXPECT_FALSE(printer.IsProtected());
    EXPECT_FALSE(printer.IsReadOnly());
    EXPECT_FALSE(printer.IsRemovable());
    EXPECT_FALSE(printer.IsRemoved());
    EXPECT_FALSE(printer.IsLockable());
    EXPECT_FALSE(printer.IsLocked());
    EXPECT_FALSE(printer.IsStoppable());
    EXPECT_FALSE(printer.IsStopped());

    EXPECT_EQ("SCSI2Pi", printer.GetVendor());
    EXPECT_EQ("SCSI PRINTER", printer.GetProduct());
    EXPECT_EQ(TestShared::GetVersion(), printer.GetRevision());
}

TEST(PrinterTest, GetDefaultParams)
{
    Printer printer(0);

    auto params = printer.GetDefaultParams();
    EXPECT_EQ(1U, params.size());
    EXPECT_EQ("lp -oraw %f", params["cmd"]);
}

TEST(PrinterTest, Init)
{
    Printer printer(0);

    param_map params;
    params["cmd"] = "missing_filename_specifier";
    EXPECT_FALSE(printer.Init(params));

    params["cmd"] = "%f";
    EXPECT_TRUE(printer.Init(params));
}

TEST(PrinterTest, TestUnitReady)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status());
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_test_unit_ready));
    EXPECT_EQ(status_code::good, controller->GetStatus());
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
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(PrinterTest, ReleaseUnit)
{
    auto [controller, printer] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status()).Times(1);
    EXPECT_NO_THROW(printer->Dispatch(scsi_command::cmd_release6));
    EXPECT_EQ(status_code::good, controller->GetStatus());
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
    EXPECT_EQ(status_code::good, controller->GetStatus());
}

TEST(PrinterTest, SynchronizeBuffer)
{
    auto [controller, printer] = CreateDevice(SCLP);

    TestShared::Dispatch(*printer, scsi_command::cmd_synchronize_buffer, sense_key::aborted_command,
        asc::printer_nothing_to_print);

    // Further testing would use the printing system
}

TEST(PrinterTest, WriteData)
{
    auto [controller, printer] = CreateDevice(SCLP);

    const vector<uint8_t> buf(1);
    controller->SetTransferSize(1, 1);
    EXPECT_NO_THROW(dynamic_pointer_cast<Printer>(printer)->WriteData(buf, scsi_command::cmd_print));
}

TEST(PrinterTest, GetStatistics)
{
    Printer printer(0);

    const auto &statistics = printer.GetStatistics();
    EXPECT_EQ(4U, statistics.size());
    EXPECT_EQ("file_print_count", statistics[0].key());
    EXPECT_EQ(0, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("byte_receive_count", statistics[1].key());
    EXPECT_EQ(0, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
    EXPECT_EQ("print_error_count", statistics[2].key());
    EXPECT_EQ(0, statistics[2].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_ERROR, statistics[2].category());
    EXPECT_EQ("print_warning_count", statistics[3].key());
    EXPECT_EQ(0, statistics[3].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_WARNING, statistics[3].category());
}

