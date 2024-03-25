//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <gmock/gmock.h>
#include "command/command_executor.h"
#include "buses/in_process_bus.h"
#include "controllers/controller.h"
#include "devices/sasi_hd.h"
#include "devices/scsi_hd.h"
#include "devices/scsi_cd.h"
#include "devices/optical_memory.h"
#include "devices/host_services.h"
#include "test_shared.h"

using namespace testing;

class MockBus : public Bus // NOSONAR Having many methods cannot be avoided
{

public:

    MOCK_METHOD(bool, Init, (bool), (override));
    MOCK_METHOD(void, Reset, (), (override));
    MOCK_METHOD(void, CleanUp, (), (override));
    MOCK_METHOD(void, SetBSY, (bool), (override));
    MOCK_METHOD(void, SetSEL, (bool), (override));
    MOCK_METHOD(bool, GetIO, (), (override));
    MOCK_METHOD(void, SetIO, (bool), (override));
    MOCK_METHOD(uint8_t, GetDAT, (), (override));
    MOCK_METHOD(void, SetDAT, (uint8_t), (override));
    MOCK_METHOD(uint32_t, Acquire, (), (override));
    MOCK_METHOD(bool, GetSignal, (int), (const, override));
    MOCK_METHOD(void, SetSignal, (int, bool), (override));
    MOCK_METHOD(bool, WaitSignal, (int, bool), (override));
    MOCK_METHOD(bool, WaitForSelection, (), (override));
    MOCK_METHOD(void, WaitBusSettle, (), (const, override));
    MOCK_METHOD(void, EnableIRQ, (), (override));
    MOCK_METHOD(void, DisableIRQ, (), (override));
};

class MockInProcessBus : public InProcessBus
{
    FRIEND_TEST(InProcessBusTest, IsTarget);

public:

    MOCK_METHOD(void, CleanUp, (), (override));
    MOCK_METHOD(void, Reset, (), (override));

    using InProcessBus::InProcessBus;

    void ResetMock()
    {
        InProcessBus::Reset();
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

inline static const auto mock_bus = make_shared<MockBus>();

class MockAbstractController : public AbstractController // NOSONAR Having many methods cannot be avoided
{
    friend class testing::TestShared;

    friend shared_ptr<PrimaryDevice> CreateDevice(s2p_interface::PbDeviceType, AbstractController&, int);

    FRIEND_TEST(AbstractControllerTest, Reset);
    FRIEND_TEST(AbstractControllerTest, Status);
    FRIEND_TEST(AbstractControllerTest, DeviceLunLifeCycle);
    FRIEND_TEST(AbstractControllerTest, ExtractInitiatorId);
    FRIEND_TEST(AbstractControllerTest, GetOpcode);
    FRIEND_TEST(AbstractControllerTest, Message);
    FRIEND_TEST(AbstractControllerTest, TransferSize);
    FRIEND_TEST(AbstractControllerTest, Length);
    FRIEND_TEST(AbstractControllerTest, UpdateOffsetAndLength);
    FRIEND_TEST(AbstractControllerTest, Offset);
    FRIEND_TEST(ControllerTest, Selection);
    FRIEND_TEST(PrimaryDeviceTest, CheckReservation);
    FRIEND_TEST(PrimaryDeviceTest, Inquiry);
    FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
    FRIEND_TEST(PrimaryDeviceTest, RequestSense);
    FRIEND_TEST(PrimaryDeviceTest, SendDiagnostic);
    FRIEND_TEST(PrimaryDeviceTest, ReportLuns);
    FRIEND_TEST(PrimaryDeviceTest, UnknownCommand);
    FRIEND_TEST(ModePageDeviceTest, ModeSense6);
    FRIEND_TEST(ModePageDeviceTest, ModeSense10);
    FRIEND_TEST(ModePageDeviceTest, ModeSelect6);
    FRIEND_TEST(ModePageDeviceTest, ModeSelect10);
    FRIEND_TEST(DiskTest, Dispatch);
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
    FRIEND_TEST(DiskTest, ReadLong10);
    FRIEND_TEST(DiskTest, ReadLong16);
    FRIEND_TEST(DiskTest, WriteLong10);
    FRIEND_TEST(DiskTest, WriteLong16);
    FRIEND_TEST(DiskTest, PreventAllowMediumRemoval);
    FRIEND_TEST(DiskTest, SynchronizeCache);
    FRIEND_TEST(DiskTest, ReadDefectData);
    FRIEND_TEST(DiskTest, StartStopUnit);
    FRIEND_TEST(DiskTest, ModeSense6);
    FRIEND_TEST(DiskTest, ModeSense10);
    FRIEND_TEST(ScsiHdTest, ModeSense6);
    FRIEND_TEST(ScsiHdTest, ModeSense10);
    FRIEND_TEST(ScsiCdTest, ReadToc);
    FRIEND_TEST(ScsiDaynaportTest, Read);
    FRIEND_TEST(ScsiDaynaportTest, Write);
    FRIEND_TEST(ScsiDaynaportTest, Read6);
    FRIEND_TEST(ScsiDaynaportTest, Write6);
    FRIEND_TEST(ScsiDaynaportTest, TestRetrieveStats);
    FRIEND_TEST(ScsiDaynaportTest, SetInterfaceMode);
    FRIEND_TEST(ScsiDaynaportTest, SetMcastAddr);
    FRIEND_TEST(ScsiDaynaportTest, EnableInterface);
    FRIEND_TEST(HostServicesTest, StartStopUnit);
    FRIEND_TEST(HostServicesTest, ExecuteOperation);
    FRIEND_TEST(HostServicesTest, ReceiveOperationResults);
    FRIEND_TEST(HostServicesTest, ModeSense6);
    FRIEND_TEST(HostServicesTest, ModeSense10);
    FRIEND_TEST(HostServicesTest, SetUpModePages);
    FRIEND_TEST(PrinterTest, Print);
    FRIEND_TEST(SasiHdTest, Inquiry);
    FRIEND_TEST(SasiHdTest, RequestSense);

public:

    MOCK_METHOD(bool, Process, (), (override));
    MOCK_METHOD(int, GetEffectiveLun, (), (const, override));
    MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));
    MOCK_METHOD(void, Status, (), (override));
    MOCK_METHOD(void, DataIn, (), (override));
    MOCK_METHOD(void, DataOut, (), (override));
    MOCK_METHOD(void, BusFree, (), (override));
    MOCK_METHOD(void, Selection, (), (override));
    MOCK_METHOD(void, Command, (), (override));
    MOCK_METHOD(void, MsgIn, (), (override));
    MOCK_METHOD(void, MsgOut, (), (override));

    MockAbstractController() : AbstractController(*mock_bus, 0, 32)
    {
    }
    explicit MockAbstractController(int target_id) : AbstractController(*mock_bus, target_id, 32)
    {
        SetCurrentLength(512);
    }
    MockAbstractController(shared_ptr<Bus> bus, int target_id) : AbstractController(*bus, target_id, 32)
    {
        SetCurrentLength(512);
    }
    ~MockAbstractController() override = default;
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
    FRIEND_TEST(PrimaryDeviceTest, RequestSense);

public:

    MOCK_METHOD(void, Reset, (), (override));
    MOCK_METHOD(void, Status, (), (override));
    MOCK_METHOD(void, Execute, (), ());

    using Controller::Controller;
    MockController(shared_ptr<Bus> bus, int target_id) : Controller(*bus, target_id, 32)
    {
    }
    explicit MockController(shared_ptr<Bus> bus) : Controller(*bus, 0, 32)
    {
    }
    ~MockController() override = default;
};

class MockDevice : public Device
{
    FRIEND_TEST(DeviceTest, Properties);
    FRIEND_TEST(DeviceTest, Params);
    FRIEND_TEST(DeviceTest, StatusCode);
    FRIEND_TEST(DeviceTest, Reset);
    FRIEND_TEST(DeviceTest, Start);
    FRIEND_TEST(DeviceTest, Stop);
    FRIEND_TEST(DeviceTest, Eject);

public:

    MOCK_METHOD(int, GetId, (), (const, override));

    explicit MockDevice(int lun) : Device(UNDEFINED, lun)
    {
    }
    explicit MockDevice(PbDeviceType type) : Device(type, 0)
    {
    }
    ~MockDevice() override = default;
};

class MockPrimaryDevice : public PrimaryDevice
{
    FRIEND_TEST(PrimaryDeviceTest, StatusPhase);
    FRIEND_TEST(PrimaryDeviceTest, DataInPhase);
    FRIEND_TEST(PrimaryDeviceTest, DataOutPhase);
    FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
    FRIEND_TEST(PrimaryDeviceTest, RequestSense);
    FRIEND_TEST(PrimaryDeviceTest, Inquiry);
    FRIEND_TEST(ControllerTest, RequestSense);
    FRIEND_TEST(CommandExecutorTest, ValidateOperationAgainstDevice);

public:

    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));
    MOCK_METHOD(void, FlushCache, (), (override));

    explicit MockPrimaryDevice(int lun) : PrimaryDevice(UNDEFINED, scsi_level::scsi_2, lun)
    {
    }
    ~MockPrimaryDevice() override = default;
};

class MockModePageDevice : public ModePageDevice
{
    FRIEND_TEST(ModePageDeviceTest, SupportsSaveParameters);
    FRIEND_TEST(ModePageDeviceTest, AddModePages);
    FRIEND_TEST(ModePageDeviceTest, AddVendorPages);

public:

    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));
    MOCK_METHOD(int, ModeSense6, (span<const int>, vector<uint8_t>&), (const, override));
    MOCK_METHOD(int, ModeSense10, (span<const int>, vector<uint8_t>&), (const, override));

    MockModePageDevice() : ModePageDevice(UNDEFINED, scsi_level::scsi_2, 0, false, false)
    {
    }
    ~MockModePageDevice() override = default;

    void SetUpModePages(map<int, vector<byte>> &pages, int page, bool) const override
    {
        // Return dummy data for other pages than page 0
        if (page) {
            vector<byte> buf(32);
            pages[page] = buf;
        }
    }
};

class MockStorageDevice : public StorageDevice
{
    FRIEND_TEST(StorageDeviceTest, ValidateFile);
    FRIEND_TEST(StorageDeviceTest, MediumChanged);
    FRIEND_TEST(StorageDeviceTest, GetIdsForReservedFile);
    FRIEND_TEST(StorageDeviceTest, FileExists);
    FRIEND_TEST(StorageDeviceTest, GetFileSize);

public:

    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));
    MOCK_METHOD(void, Open, (), (override));
    MOCK_METHOD(int, ModeSense6, (span<const int>, vector<uint8_t>&), (const, override));
    MOCK_METHOD(int, ModeSense10, (span<const int>, vector<uint8_t>&), (const, override));
    MOCK_METHOD(void, SetUpModePages, ((map<int, vector<byte>>&), int, bool), (const, override));

    MockStorageDevice() : StorageDevice(UNDEFINED, scsi_level::scsi_2, 0, false, false)
    {
    }
    ~MockStorageDevice() override = default;
};

class MockDisk : public Disk
{
    FRIEND_TEST(DiskTest, Dispatch);
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
    FRIEND_TEST(DiskTest, ReadLong10);
    FRIEND_TEST(DiskTest, ReadLong16);
    FRIEND_TEST(DiskTest, WriteLong10);
    FRIEND_TEST(DiskTest, WriteLong16);
    FRIEND_TEST(DiskTest, ReserveRelease);
    FRIEND_TEST(DiskTest, SendDiagnostic);
    FRIEND_TEST(DiskTest, StartStopUnit);
    FRIEND_TEST(DiskTest, PreventAllowMediumRemoval);
    FRIEND_TEST(DiskTest, Eject);
    FRIEND_TEST(DiskTest, AddAppleVendorPage);
    FRIEND_TEST(DiskTest, ModeSense6);
    FRIEND_TEST(DiskTest, ModeSense10);
    FRIEND_TEST(DiskTest, EvaluateBlockDescriptors);
    FRIEND_TEST(DiskTest, VerifySectorSizeChange);
    FRIEND_TEST(DiskTest, SynchronizeCache);
    FRIEND_TEST(DiskTest, ReadDefectData);
    FRIEND_TEST(DiskTest, BlockCount);
    FRIEND_TEST(DiskTest, SetSectorSizeInBytes);
    FRIEND_TEST(DiskTest, ChangeSectorSize);

public:

    MOCK_METHOD(vector<uint8_t>, InquiryInternal, (), (const, override));
    MOCK_METHOD(void, FlushCache, (), (override));
    MOCK_METHOD(void, Open, (), (override));

    MockDisk() : Disk(SCHD, scsi_level::scsi_2, 0, false, false, { 512, 1024, 2048, 4096 })
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
    ~MockDisk() override = default;
};

class MockSasiHd : public SasiHd // NOSONAR Ignore inheritance hierarchy depth in unit tests
{
public:

    explicit MockSasiHd(int lun) : SasiHd(lun)
    {
    }
    explicit MockSasiHd(const unordered_set<uint32_t> &sector_sizes) : SasiHd(0, sector_sizes)
    {
    }
    ~MockSasiHd() override = default;
};

class MockScsiHd : public ScsiHd // NOSONAR Ignore inheritance hierarchy depth in unit tests
{
    FRIEND_TEST(DiskTest, ConfiguredSectorSize);
    FRIEND_TEST(ScsiHdTest, SupportsSaveParameters);
    FRIEND_TEST(ScsiHdTest, FinalizeSetup);
    FRIEND_TEST(ScsiHdTest, GetProductData);
    FRIEND_TEST(ScsiHdTest, SetUpModePages);
    FRIEND_TEST(ScsiHdTest, GetSectorSizes);
    FRIEND_TEST(ScsiHdTest, ModeSense6);
    FRIEND_TEST(ScsiHdTest, ModeSense10);
    FRIEND_TEST(ScsiHdTest, ModeSelect);
    FRIEND_TEST(ScsiHdTest, ModeSelect6_Single);
    FRIEND_TEST(ScsiHdTest, ModeSelect6_Multiple);
    FRIEND_TEST(ScsiHdTest, ModeSelect10_Single);
    FRIEND_TEST(ScsiHdTest, ModeSelect10_Multiple);
    FRIEND_TEST(CommandExecutorTest, ProcessDeviceCmd);

public:

    MockScsiHd(int lun, bool removable) : ScsiHd(lun, removable, false, false)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
    explicit MockScsiHd(const unordered_set<uint32_t> &sector_sizes)
    : ScsiHd(0, false, false, false, sector_sizes)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
    ~MockScsiHd() override = default;
};

class MockScsiCd : public ScsiCd // NOSONAR Ignore inheritance hierarchy depth in unit tests
{
    FRIEND_TEST(ScsiCdTest, GetSectorSizes);
    FRIEND_TEST(ScsiCdTest, SetUpModePages);
    FRIEND_TEST(ScsiCdTest, ReadToc);

public:

    explicit MockScsiCd(int lun) : ScsiCd(lun, false)
    {
        SetCachingMode(PbCachingMode::PISCSI);
    }
    ~MockScsiCd() override = default;
};

class MockOpticalMemory : public OpticalMemory // NOSONAR Ignore inheritance hierarchy depth in unit tests
{
    FRIEND_TEST(OpticalMemoryTest, SupportsSaveParameters);
    FRIEND_TEST(OpticalMemoryTest, SetUpModePages);
    FRIEND_TEST(OpticalMemoryTest, AddVendorPages);
    FRIEND_TEST(OpticalMemoryTest, ModeSelect);

    using OpticalMemory::OpticalMemory;
};

class MockHostServices : public HostServices
{
    FRIEND_TEST(HostServicesTest, SetUpModePages);

    using HostServices::HostServices;
};

class MockCommandExecutor : public CommandExecutor
{
public:

    MOCK_METHOD(bool, Start, (shared_ptr<PrimaryDevice>, bool), (const));
    MOCK_METHOD(bool, Stop, (shared_ptr<PrimaryDevice>, bool), (const));

    using CommandExecutor::CommandExecutor;
};
