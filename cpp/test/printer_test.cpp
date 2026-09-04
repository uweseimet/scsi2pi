//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/printer.h"
#include "shared/s2p_exceptions.h"

class PrinterTest : public ::testing::Test {
public:

    void SetUp() override {
        tie(controller, printer) = CreateDevice(SCLP);
    }

    void TearDown() override {
        controller.reset();
        printer.reset();
    }

    shared_ptr<MockAbstractController> controller;
    shared_ptr<PrimaryDevice> printer;
};

TEST_F(PrinterTest, Device_Defaults)
{
    EXPECT_EQ(SCLP, printer->GetType());
    EXPECT_FALSE(printer->SupportsImageFile());
    EXPECT_TRUE(printer->SupportsParams());
    EXPECT_FALSE(printer->IsProtectable());
    EXPECT_FALSE(printer->IsProtected());
    EXPECT_FALSE(printer->IsReadOnly());
    EXPECT_FALSE(printer->IsRemovable());
    EXPECT_FALSE(printer->IsRemoved());
    EXPECT_FALSE(printer->IsLocked());
    EXPECT_FALSE(printer->IsStoppable());
    EXPECT_FALSE(printer->IsStopped());

    const auto& [vendor, product, revision] = printer->GetProductData();
    EXPECT_EQ("SCSI2Pi", vendor);
    EXPECT_EQ("SCSI PRINTER", product);
    EXPECT_EQ(TestShared::GetVersion(), revision);
}

TEST_F(PrinterTest, GetDefaultParams)
{
    const auto &params = printer->GetDefaultParams();
    EXPECT_EQ(1U, params.size());
    EXPECT_EQ("lp -oraw %f", params.at("cmd"));
}

TEST_F(PrinterTest, GetIdentifier)
{
    EXPECT_EQ("SCSI Printer", printer->GetIdentifier());
}

TEST_F(PrinterTest, Init)
{
    Printer p(0);

    param_map params;
    params["cmd"] = "%f";
    p.SetParams(params);
    EXPECT_EQ("", p.Init());
}

TEST_F(PrinterTest, TestUnitReady)
{
    EXPECT_CALL(*controller, Status);
    Dispatch(printer, ScsiCommand::TEST_UNIT_READY);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST_F(PrinterTest, Inquiry)
{
    TestShared::Inquiry(SCLP, DeviceType::PRINTER, ScsiLevel::SCSI_2, "SCSI2Pi SCSI PRINTER    ", 0x1f, false);
}

TEST_F(PrinterTest, ReserveUnit)
{
    EXPECT_CALL(*controller, Status);
    Dispatch(printer, ScsiCommand::RESERVE_RESERVE_ELEMENT_6);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST_F(PrinterTest, ReleaseUnit)
{
    EXPECT_CALL(*controller, Status);
    Dispatch(printer, ScsiCommand::RELEASE_RELEASE_ELEMENT_6);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST_F(PrinterTest, Print)
{
    EXPECT_CALL(*controller, DataOut).Times(AtLeast(1));
    Dispatch(printer, ScsiCommand::PRINT);

    controller->SetCdbByte(2, 0xff);
    controller->SetCdbByte(3, 0xff);
    controller->SetCdbByte(4, 0xff);
    Dispatch(printer, ScsiCommand::PRINT, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Buffer overflow was not reported");
}

TEST_F(PrinterTest, StopPrint)
{
    EXPECT_CALL(*controller, Status);
    Dispatch(printer, ScsiCommand::STOP_PRINT);
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST_F(PrinterTest, SynchronizeBuffer)
{
    param_map params;
    params["cmd"] = "false %f";
    printer->SetParams(params);

    Dispatch(printer, ScsiCommand::SYNCHRONIZE_BUFFER, SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::PRINT));
    controller->SetTransferSize(4, 4);
    printer->WriteData(controller->GetCdb(), controller->GetBuffer(), 4);
    Dispatch(printer, ScsiCommand::SYNCHRONIZE_BUFFER, SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);
}

TEST_F(PrinterTest, WriteData)
{
    controller->SetTransferSize(4, 4);
    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::CLOSE_TRACK_SESSION));
    EXPECT_THROW(printer->WriteData(controller->GetCdb(), controller->GetBuffer(), 4), ScsiException);

    controller->SetCdbByte(0, static_cast<int>(ScsiCommand::PRINT));
    printer->WriteData(controller->GetCdb(), controller->GetBuffer(), 4);
}

TEST_F(PrinterTest, GetStatistics)
{
    const auto &statistics = printer->GetStatistics();
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
