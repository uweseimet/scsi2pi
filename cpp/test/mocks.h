//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <gmock/gmock.h>
#include "buses/bus.h"
#include "controllers/controller.h"
#include "devices/optical_memory.h"
#include "devices/sasi_hd.h"
#include "devices/scsi_cd.h"
#include "devices/scsi_hd.h"
#include "devices/tape.h"
#include "test_shared.h"

using namespace testing;

class MockBus : public Bus
{

public:

    MOCK_METHOD(void, SetBSY, (bool), (const, override));
    MOCK_METHOD(void, SetDAT, (uint8_t), (const, override));
    MOCK_METHOD(void, Acquire, (), (const, override));
    MOCK_METHOD(void, SetSignal, (int, bool), (const, override));
    MOCK_METHOD(void, SetDir, (bool), (const, override));
    MOCK_METHOD(bool, WaitHandShake, (int, bool), (const, override));
    MOCK_METHOD(uint8_t, WaitForSelection, (), (override));
    MOCK_METHOD(void, WaitNanoSeconds, (bool), (const, override));
    MOCK_METHOD(void, EnableIRQ, (), (override));
    MOCK_METHOD(void, DisableIRQ, (), (override));
    MOCK_METHOD(bool, IsRaspberryPi, (), (const, override));

    MockBus()
    {
        SetSignals(0xffffffff);
    }
};

class MockPhaseHandler : public PhaseHandler
{
    FRIEND_TEST(PhaseHandlerTest, Phases);
    FRIEND_TEST(PhaseHandlerTest, ProcessPhase);

public:

    MOCK_METHOD(void, Status, (), (override));
    MOCK_METHOD(void, DataIn, (), (override));
    MOCK_METHOD(void, DataOut, (), (override));
    MOCK_METHOD(void, BusFree, (), (override));
    MOCK_METHOD(void, Selection, (), (override));
    MOCK_METHOD(void, Command, (), (override));
    MOCK_METHOD(void, MsgIn, (), (override));
    MOCK_METHOD(void, MsgOut, (), (override));

    using PhaseHandler::PhaseHandler;
};

class MockAbstractController : public AbstractController // NOSONAR Having many methods cannot be avoided
{
    friend class testing::TestShared;

    FRIEND_TEST(AbstractControllerTest, Reset);
    FRIEND_TEST(AbstractControllerTest, Message);
    FRIEND_TEST(AbstractControllerTest, Lengths);
    FRIEND_TEST(AbstractControllerTest, UpdateOffsetAndLength);
    FRIEND_TEST(AbstractControllerTest, Offset);
    FRIEND_TEST(AbstractControllerTest, ScriptGenerator);

    const S2pFormatter formatter;

public:

    MOCK_METHOD(bool, Process, (), (override));
    MOCK_METHOD(int, GetEffectiveLun, (), (const, override));
    MOCK_METHOD(void, Error, (SenseKey, Asc, StatusCode), (override));
    MOCK_METHOD(void, Status, (), (override));
    MOCK_METHOD(void, DataIn, (), (override));
    MOCK_METHOD(void, DataOut, (), (override));
    MOCK_METHOD(void, BusFree, (), (override));
    MOCK_METHOD(void, Selection, (), (override));
    MOCK_METHOD(void, Command, (), (override));
    MOCK_METHOD(void, MsgIn, (), (override));
    MOCK_METHOD(void, MsgOut, (), (override));

    MockAbstractController() : AbstractController(0, formatter)
    {
    }
    explicit MockAbstractController(int t) : AbstractController(t, formatter)
    {
        SetCurrentLength(512);
    }
    ~MockAbstractController() override = default;

    void ResetCdb()
    {
        for (size_t i = 0; i < GetCdb().size(); ++i) {
            SetCdbByte(static_cast<int>(i), 0);
        }
    }

    void SetCdbByte(int index, int value) // NONSONAR Having the same name as the inherited method is intentional
    {
        AbstractController::SetCdbByte(index, value);
    }
};

class MockController : public Controller
{
    FRIEND_TEST(ControllerTest, Process);
    FRIEND_TEST(ControllerTest, BusFree);
    FRIEND_TEST(ControllerTest, Selection);
    FRIEND_TEST(ControllerTest, Command);
    FRIEND_TEST(ControllerTest, MsgIn);
    FRIEND_TEST(ControllerTest, MsgOut);
    FRIEND_TEST(ControllerTest, DataIn);
    FRIEND_TEST(ControllerTest, DataOut);
    FRIEND_TEST(ControllerTest, Error);
    FRIEND_TEST(ControllerTest, RequestSense);

public:

    const S2pFormatter formatter;

    MOCK_METHOD(void, Status, (), (override));

    using Controller::Controller;
    MockController(shared_ptr<Bus> b, int t) : Controller(*b, t, nullptr, formatter)
    {
    }
    explicit MockController(shared_ptr<Bus> b) : Controller(*b, 0, nullptr, formatter)
    {
    }
    ~MockController() override = default;
};

class MockDevice : public Device
{
    FRIEND_TEST(DeviceTest, Properties);
    FRIEND_TEST(DeviceTest, StatusCode);
    FRIEND_TEST(DeviceTest, Start);
    FRIEND_TEST(DeviceTest, Stop);
    FRIEND_TEST(DeviceTest, Eject);

public:

    MOCK_METHOD(int, GetId, (), (const, override));

    explicit MockDevice(int l) : Device(UNDEFINED, l)
    {
    }
    ~MockDevice() override = default;
};

class MockPrimaryDevice : public PrimaryDevice
{
    FRIEND_TEST(PrimaryDeviceTest, Reset);
    FRIEND_TEST(PrimaryDeviceTest, StatusPhase);
    FRIEND_TEST(PrimaryDeviceTest, DataInPhase);
    FRIEND_TEST(PrimaryDeviceTest, DataOutPhase);
    FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
    FRIEND_TEST(PrimaryDeviceTest, RequestSense);
    FRIEND_TEST(PrimaryDeviceTest, Inquiry);
    FRIEND_TEST(PrimaryDeviceTest, ModeSense6);
    FRIEND_TEST(PrimaryDeviceTest, ModeSense10);
    FRIEND_TEST(ControllerTest, RequestSense);
    FRIEND_TEST(CommandExecutorTest, ValidateOperation);

public:

    MOCK_METHOD(string, SetUp, (), (override));
    MOCK_METHOD(string, GetIdentifier, (), (const, override));
    MOCK_METHOD(int, WriteData, (cdb_t, data_out_t, int), (override));
    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));

    explicit MockPrimaryDevice(int l) : PrimaryDevice(UNDEFINED, l)
    {
    }
    ~MockPrimaryDevice() override = default;
};

class MockStorageDevice : public StorageDevice
{
    FRIEND_TEST(StorageDeviceTest, ValidateFile);
    FRIEND_TEST(StorageDeviceTest, CheckWritePreconditions);
    FRIEND_TEST(StorageDeviceTest, MediumChanged);
    FRIEND_TEST(StorageDeviceTest, GetIdsForReservedFile);
    FRIEND_TEST(StorageDeviceTest, GetFileSize);
    FRIEND_TEST(StroageDeviceTest, PreventAllowMediumRemoval);
    FRIEND_TEST(StorageDeviceTest, StartStopUnit);
    FRIEND_TEST(StorageDeviceTest, SetGetBlockSize);
    FRIEND_TEST(StorageDeviceTest, EvaluateBlockDescriptors);
    FRIEND_TEST(StorageDeviceTest, VerifyBlockSizeChange);
    FRIEND_TEST(StorageDeviceTest, BlockCount);
    FRIEND_TEST(StorageDeviceTest, ChangeBlockSize);
    FRIEND_TEST(StorageDeviceTest, ModeSense6);
    FRIEND_TEST(StorageDeviceTest, ModeSense10);
    FRIEND_TEST(StorageDeviceTest, GetStatistics);

public:

    MOCK_METHOD(int, WriteData, (cdb_t, data_out_t, int), (override));
    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));
    MOCK_METHOD(void, Open, (), (override));

    MockStorageDevice() : StorageDevice(UNDEFINED, 0, false, false, { 256, 512, 1024, 2048, 4096 })
    {
    }
    ~MockStorageDevice() override = default;

    void SetReady(bool b) // NONSONAR Having the same name as the inherited method is intentional
    {
        PrimaryDevice::SetReady(b);
    }
    void SetRemovable(bool b) // NONSONAR Having the same name as the inherited method is intentional
    {
        PrimaryDevice::SetRemovable(b);
    }
    void SetLocked(bool b) // NONSONAR Having the same name as the inherited method is intentional
    {
        PrimaryDevice::SetLocked(b);
    }
};

class MockDisk : public Disk
{
    FRIEND_TEST(DiskTest, Dispatch);
    FRIEND_TEST(DiskTest, FinalizeSetup);
    FRIEND_TEST(DiskTest, ValidateFile);
    FRIEND_TEST(DiskTest, Rezero);
    FRIEND_TEST(DiskTest, FormatUnit);
    FRIEND_TEST(DiskTest, ReassignBlocks);
    FRIEND_TEST(DiskTest, Seek6);
    FRIEND_TEST(DiskTest, Seek10);
    FRIEND_TEST(DiskTest, Read6);
    FRIEND_TEST(DiskTest, Read10);
    FRIEND_TEST(DiskTest, Read16);
    FRIEND_TEST(DiskTest, Write6);
    FRIEND_TEST(DiskTest, Write10);
    FRIEND_TEST(DiskTest, Write16);
    FRIEND_TEST(DiskTest, Verify10);
    FRIEND_TEST(DiskTest, Verify16);
    FRIEND_TEST(DiskTest, ReadCapacity10);
    FRIEND_TEST(DiskTest, ReadCapacity16);
    FRIEND_TEST(DiskTest, ReadFormatCapacities);
    FRIEND_TEST(DiskTest, ReadLong10);
    FRIEND_TEST(DiskTest, ReadLong16);
    FRIEND_TEST(DiskTest, WriteLong10);
    FRIEND_TEST(DiskTest, WriteLong16);
    FRIEND_TEST(DiskTest, ReserveRelease);
    FRIEND_TEST(DiskTest, SendDiagnostic);
    FRIEND_TEST(DiskTest, Eject);
    FRIEND_TEST(DiskTest, AddAppleVendorPage);
    FRIEND_TEST(DiskTest, ModeSense6);
    FRIEND_TEST(DiskTest, ModeSense10);
    FRIEND_TEST(DiskTest, SynchronizeCache);
    FRIEND_TEST(DiskTest, ReadDefectData);
    FRIEND_TEST(DiskTest, ChangeBlockSize);

public:

    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));
    MOCK_METHOD(void, FlushCache, (), (override));
    MOCK_METHOD(void, Open, (), (override));

    MockDisk() : Disk(SCHD, 0, false, false, { 512, 1024, 2048, 4096 })
    {
        SetCachingMode(PbCachingMode::LINUX);
        SetBlockSize(512);
    }
    ~MockDisk() override = default;
};

class MockScsiHd : public ScsiHd
{
    FRIEND_TEST(ScsiHdTest, GetProductData);
    FRIEND_TEST(ScsiHdTest, ModeSense6);
    FRIEND_TEST(ScsiHdTest, ModeSense10);
    FRIEND_TEST(ScsiHdTest, ModeSelect);
    FRIEND_TEST(ScsiHdTest, ModeSelect6_Single);
    FRIEND_TEST(ScsiHdTest, ModeSelect6_Multiple);
    FRIEND_TEST(ScsiHdTest, ModeSelect10_Single);
    FRIEND_TEST(ScsiHdTest, ModeSelect10_Multiple);
    FRIEND_TEST(CommandExecutorTest, ProcessDeviceCmd);

public:

    MockScsiHd(int l, bool r) : ScsiHd(l, r, false, false)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
    explicit MockScsiHd(const set<uint32_t> &sector_sizes)
    : ScsiHd(0, false, false, false, sector_sizes)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
    ~MockScsiHd() override = default;
};

class MockScsiCd : public ScsiCd
{
    FRIEND_TEST(ScsiCdTest, ReadToc);

public:

    explicit MockScsiCd(int l) : ScsiCd(l, false)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
};

class MockOpticalMemory : public OpticalMemory
{
    FRIEND_TEST(OpticalMemoryTest, AddVendorPages);
    FRIEND_TEST(OpticalMemoryTest, ModeSelect);

public:

    explicit MockOpticalMemory(int l) : OpticalMemory(l)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
};

class MockTape : public Tape
{
    FRIEND_TEST(TapeTest, ValidateFile);
    FRIEND_TEST(TapeTest, Unload);
    FRIEND_TEST(TapeTest, ModeSense6);
    FRIEND_TEST(TapeTest, ModeSense10);
    FRIEND_TEST(TapeTest, VerifyBlockSizeChange);
    FRIEND_TEST(TapeTest, ReadPosition);

public:

    MockTape() : Tape(0)
    {
    }
};
