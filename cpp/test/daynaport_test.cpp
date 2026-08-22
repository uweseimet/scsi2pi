//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/daynaport.h"
#include "shared/s2p_exceptions.h"

class DaynaportTest : public ::testing::Test {
protected:

    void SetUp() override {
        tie(controller, daynaport) = CreateDevice(SCDP);
    }

    void TearDown() override {
        controller.reset();
        daynaport.reset();
    }

    shared_ptr<MockAbstractController> controller;
    shared_ptr<PrimaryDevice> daynaport;
};

TEST_F(DaynaportTest, Device_Defaults)
{
    EXPECT_EQ(SCDP, daynaport->GetType());
    EXPECT_FALSE(daynaport->SupportsImageFile());
    EXPECT_TRUE(daynaport->SupportsParams());
    EXPECT_FALSE(daynaport->IsProtectable());
    EXPECT_FALSE(daynaport->IsProtected());
    EXPECT_FALSE(daynaport->IsReadOnly());
    EXPECT_FALSE(daynaport->IsRemovable());
    EXPECT_FALSE(daynaport->IsRemoved());
    EXPECT_FALSE(daynaport->IsLocked());
    EXPECT_FALSE(daynaport->IsStoppable());
    EXPECT_FALSE(daynaport->IsStopped());

    const auto& [vendor, product, revision] = daynaport->GetProductData();
    EXPECT_EQ("Dayna", vendor);
    EXPECT_EQ("SCSI/Link", product);
    EXPECT_EQ("1.4a", revision);
}

TEST_F(DaynaportTest, GetDefaultParams)
{
    const auto &params = daynaport->GetDefaultParams();
    EXPECT_EQ(3U, params.size());
    EXPECT_TRUE(params.contains("interface"));
    EXPECT_TRUE(params.contains("inet"));
    EXPECT_EQ("true", params.at("bridge"));
}

TEST_F(DaynaportTest, GetIdentifier)
{
    EXPECT_EQ("DaynaPort SCSI/Link", daynaport->GetIdentifier());
}

TEST_F(DaynaportTest, Inquiry)
{
    TestShared::Inquiry(SCDP, DeviceType::PROCESSOR, ScsiLevel::SCSI_2, "Dayna   SCSI/Link       1.4a", 0x1f, false);
}

TEST_F(DaynaportTest, HandleInquiry)
{
    controller->SetCdbByte(4, 255);
    EXPECT_EQ(36U, dynamic_pointer_cast<DaynaPort>(daynaport)->HandleInquiry().size());

    controller->SetCdbByte(4, 37);
    EXPECT_EQ(37U, dynamic_pointer_cast<DaynaPort>(daynaport)->HandleInquiry().size());
}

TEST_F(DaynaportTest, TestUnitReady)
{
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::TEST_UNIT_READY));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());
}

TEST_F(DaynaportTest, WriteData)
{
    array<int, 6> cdb = { };
    const array<const uint8_t, 5> buf = { };

    cdb[0] = static_cast<int>(ScsiCommand::SEND_MESSAGE_6);

    cdb[5] = 0x00;
    EXPECT_EQ(0, daynaport->WriteData(cdb, buf, 0));

    cdb[5] = 0x80;
    EXPECT_EQ(0, daynaport->WriteData(cdb, buf, 0));

    // Unknown data format must be ignored
    cdb[5] = 0xff;
    EXPECT_EQ(123, daynaport->WriteData(cdb, buf, 123));
}

TEST_F(DaynaportTest, GetMessage6)
{
    controller->SetCdbByte(4, 0x01);
    controller->SetCdbByte(5, 0xc0);
    controller->GetBuffer()[0] = 0x12;
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::GET_MESSAGE_6));
    EXPECT_EQ(0x12, controller->GetBuffer()[0]) << "No data must be returned when trying to read the root sector";

    controller->SetCdbByte(4, 0x01);
    controller->SetCdbByte(5, 0x80);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::GET_MESSAGE_6));
    EXPECT_EQ(0x12, controller->GetBuffer()[0]) << "No data must be returned when trying to read the root sector";

    controller->SetCdbByte(4, 0x00);
    controller->SetCdbByte(5, 0xff);
    Dispatch(daynaport, ScsiCommand::GET_MESSAGE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Invalid data format");
}

TEST_F(DaynaportTest, SendMessage6)
{
    controller->SetCdbByte(5, 0x00);
    Dispatch(daynaport, ScsiCommand::SEND_MESSAGE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Invalid transfer length");

    controller->SetCdbByte(3, 255);
    controller->SetCdbByte(4, 0);
    controller->SetCdbByte(5, 0x08);
    Dispatch(daynaport, ScsiCommand::SEND_MESSAGE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Invalid transfer length");

    controller->SetCdbByte(3, 0);
    controller->SetCdbByte(4, 0);
    controller->SetCdbByte(5, 0xff);
    Dispatch(daynaport, ScsiCommand::SEND_MESSAGE_6, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Invalid transfer length");

    controller->SetCdbByte(5, 0x80);
    EXPECT_CALL(*controller, DataOut);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::SEND_MESSAGE_6));
}

TEST_F(DaynaportTest, TestRetrieveStats)
{
    // ALLOCATION LENGTH
    controller->SetCdbByte(4, 255);
    EXPECT_CALL(*controller, DataIn);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::RETRIEVE_STATS));
}

TEST_F(DaynaportTest, SetInterfaceMode)
{
    Dispatch(daynaport, ScsiCommand::SET_IFACE_MODE, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Unknown interface command");

    // Not implemented, do nothing
    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SETMODE);
    EXPECT_CALL(*controller, Status);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::SET_IFACE_MODE));
    EXPECT_EQ(StatusCode::GOOD, controller->GetStatus());

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SETMAC);
    EXPECT_CALL(*controller, DataOut);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::SET_IFACE_MODE));

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_STATS);
    Dispatch(daynaport, ScsiCommand::SET_IFACE_MODE, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Not implemented");

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_ENABLE);
    Dispatch(daynaport, ScsiCommand::SET_IFACE_MODE, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Not implemented");

    controller->SetCdbByte(5, DaynaPort::CMD_SCSILINK_SET);
    Dispatch(daynaport, ScsiCommand::SET_IFACE_MODE, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Not implemented");
}

TEST_F(DaynaportTest, SetMcastAddr)
{
    Dispatch(daynaport, ScsiCommand::SET_MCAST_ADDR, SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB,
        "Length of 0 is not supported");

    controller->SetCdbByte(4, 1);
    EXPECT_CALL(*controller, DataOut);
    EXPECT_NO_THROW(Dispatch(daynaport, ScsiCommand::SET_MCAST_ADDR));
}

TEST_F(DaynaportTest, EnableInterface)
{
    controller->SetCdbByte(5, 0x80);
    Dispatch(daynaport, ScsiCommand::ENABLE_INTERFACE, SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
}

TEST_F(DaynaportTest, DisableInterface)
{
    controller->SetCdbByte(5, 0x00);
    Dispatch(daynaport, ScsiCommand::ENABLE_INTERFACE, SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
}

TEST_F(DaynaportTest, GetDelayAfterBytes)
{
    EXPECT_EQ(6, daynaport->GetDelayAfterBytes());
}

TEST_F(DaynaportTest, GetStatistics)
{
    const auto &statistics = daynaport->GetStatistics();
    EXPECT_EQ(2U, statistics.size());
    EXPECT_EQ("byte_read_count", statistics[0].key());
    EXPECT_EQ(0U, statistics[0].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[0].category());
    EXPECT_EQ("byte_write_count", statistics[1].key());
    EXPECT_EQ(0U, statistics[1].value());
    EXPECT_EQ(PbStatisticsCategory::CATEGORY_INFO, statistics[1].category());
}
