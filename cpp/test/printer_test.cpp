//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/printer.h"
#include "shared/s2p_exceptions.h"

TEST(PrinterTest, Device_Defaults)
{
    Printer PRINTER(0);

    EXPECT_EQ(SCLP, PRINTER.GetType());
    EXPECT_FALSE(PRINTER.SupportsImageFile());
    EXPECT_TRUE(PRINTER.SupportsParams());
    EXPECT_FALSE(PRINTER.IsProtectable());
    EXPECT_FALSE(PRINTER.IsProtected());
    EXPECT_FALSE(PRINTER.IsReadOnly());
    EXPECT_FALSE(PRINTER.IsRemovable());
    EXPECT_FALSE(PRINTER.IsRemoved());
    EXPECT_FALSE(PRINTER.IsLocked());
    EXPECT_FALSE(PRINTER.IsStoppable());
    EXPECT_FALSE(PRINTER.IsStopped());

    const auto& [vendor, product, revision] = PRINTER.GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("SCSI PRINTER", product);
    EXPECT_EQ(TestShared::GetVersion(), revision);
}

TEST(PrinterTest, GetDefaultParams)
{
    Printer PRINTER(0);

    const auto &params = PRINTER.GetDefaultParams();
    EXPECT_EQ(1U, params.size());
    EXPECT_EQ("lp -oraw %f", params.at("cmd"));
}

TEST(PrinterTest, GetIdentifier)
{
    Printer PRINTER(0);

    EXPECT_EQ("SCSI Printer", PRINTER.GetIdentifier());
}

TEST(PrinterTest, Init)
{
    Printer PRINTER(0);

    param_map params;
    params["cmd"] = "%f";
    PRINTER.SetParams(params);
    EXPECT_EQ("", PRINTER.Init());
}

TEST(PrinterTest, TestUnitReady)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(PRINTER, ScsiCommand::TEST_UNIT_READY));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST(PrinterTest, Inquiry)
{
    TestShared::Inquiry(SCLP, DeviceType::PRINTER, ScsiLevel::SCSI_2, "SCSI2Pi SCSI PRINTER    ", 0x1f, false);
}

TEST(PrinterTest, ReserveUnit)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status).Times(1);
    EXPECT_NO_THROW(Dispatch(PRINTER, ScsiCommand::RESERVE_RESERVE_ELEMENT_6));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST(PrinterTest, ReleaseUnit)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status).Times(1);
    EXPECT_NO_THROW(Dispatch(PRINTER, ScsiCommand::RELEASE_RELEASE_ELEMENT_6));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST(PrinterTest, Print)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, DataOut);
    EXPECT_NO_THROW(Dispatch(PRINTER, ScsiCommand::PRINT));

    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    Dispatch(PRINTER, ScsiCommand::PRINT, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Buffer overflow was not reported");
}

TEST(PrinterTest, StopPrint)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(PRINTER, ScsiCommand::STOP_PRINT));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST(PrinterTest, SynchronizeBuffer)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    param_map params;
    params["cmd"] = "false %f";
    PRINTER->SetParams(params);

    Dispatch(PRINTER, ScsiCommand::SYNCHRONIZE_BUFFER, SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::PRINT));
    controller->SetTransferSize(4, 4);
    EXPECT_NO_THROW(PRINTER->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 4));
    Dispatch(PRINTER, ScsiCommand::SYNCHRONIZE_BUFFER, SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);

    params["cmd"] = "true %f";
    PRINTER->SetParams(params);
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::PRINT));
    controller->SetTransferSize(4, 4);
    EXPECT_NO_THROW(PRINTER->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 4));
    EXPECT_NO_THROW(Dispatch(PRINTER, ScsiCommand::SYNCHRONIZE_BUFFER));
}

TEST(PrinterTest, WriteData)
{
    auto [controller, PRINTER] = CreateDevice(SCLP);

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::PRINT));
    controller->SetTransferSize(4, 4);
    EXPECT_NO_THROW(PRINTER->WriteData(controller->GetCdb(), controller->GetBuffer(), 0, 4));
}

TEST(PrinterTest, GetStatistics)
{
    Printer PRINTER(0);

    const auto &statistics = PRINTER.GetStatistics();
    EXPECT_EQ(4U, statistics.size());
    EXPECT_EQ("file_print_count", statistics[0].key());
    EXPECT_EQ(0U, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("byte_receive_count", statistics[1].key());
    EXPECT_EQ(0U, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
    EXPECT_EQ("print_error_count", statistics[2].key());
    EXPECT_EQ(0U, statistics[2].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_ERROR, statistics[2].category());
    EXPECT_EQ("print_warning_count", statistics[3].key());
    EXPECT_EQ(0U, statistics[3].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_WARNING, statistics[3].category());
}
