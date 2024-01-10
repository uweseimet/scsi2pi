//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2020 akuker
// Copyright (C) 2023 Uwe Seimet
//
// This design is derived from the SLINKCMD.TXT file, as well as David Kuder's
// Tiny SCSI Emulator
//    - SLINKCMD: http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/SLINKCMD.TXT
//    - Tiny SCSI : https://hackaday.io/project/18974-tiny-scsi-emulator
//
// Additional documentation and clarification is available at the
// following link:
//    - https://github.com/PiSCSI/piscsi/wiki/Dayna-Port-SCSI-Link
//
// Note: This requires a DaynaPort SCSI Link driver. It has successfully been tested with MacOS and the Atari.
//
//---------------------------------------------------------------------------

#include "shared/shared_exceptions.h"
#include "shared/network_util.h"
#include "base/memory_util.h"
#include "daynaport.h"

using namespace scsi_defs;
using namespace memory_util;
using namespace network_util;

DaynaPort::DaynaPort(int lun) : PrimaryDevice(SCDP, lun)
{
    // These data are required by the DaynaPort drivers
    SetVendor("Dayna");
    SetProduct("SCSI/Link");
    SetRevision("1.4a");

    SupportsParams(true);
}

bool DaynaPort::Init(const param_map &params)
{
    PrimaryDevice::Init(params);

    AddCommand(scsi_command::cmd_test_unit_ready, [this]
        {
            TestUnitReady();
        });
    AddCommand(scsi_command::cmd_read6, [this]
        {
            Read6();
        });
    AddCommand(scsi_command::cmd_write6, [this]
        {
            Write6();
        });
    AddCommand(scsi_command::cmd_retrieve_stats, [this]
        {
            RetrieveStatistics();
        });
    AddCommand(scsi_command::cmd_set_iface_mode, [this]
        {
            SetInterfaceMode();
        });
    AddCommand(scsi_command::cmd_set_mcast_addr, [this]
        {
            SetMcastAddr();
        });
    AddCommand(scsi_command::cmd_enable_interface, [this]
        {
            EnableInterface();
        });

    tap_enabled = tap.Init(GetParams());
    if (!tap_enabled) {
// Not terminating on a regular PC is helpful for testing
#if !defined(__x86_64__) && !defined(__X86__)
		return false;
#endif
    } else {
        LogTrace("Created tap interface");
    }

    Reset();
    SetReady(true);
    SetReset(false);

    return true;
}

void DaynaPort::CleanUp()
{
    tap.CleanUp();
}

vector<uint8_t> DaynaPort::InquiryInternal()
{
    vector<uint8_t> buf = HandleInquiry(device_type::processor, scsi_level::scsi_2, false);

    if (GetController()->GetCmdByte(4) == 37) {
        macos_seen = true;

        // The Daynaport driver for the Mac expects 37 bytes: Increase additional length and
        // add a vendor-specific byte in order to satisfy this driver.
        buf[4]++;
        buf.push_back(0);
    }

    return buf;
}

//---------------------------------------------------------------------------
//
//	READ
//
//   Command:  08 00 00 LL LL XX (LLLL is data length, XX = c0 or 80)
//   Function: Read a packet at a time from the device (standard SCSI Read)
//   Type:     Input; the following data is returned:
//             LL LL NN NN NN NN XX XX XX ... CC CC CC CC
//   where:
//             LLLL      is normally the length of the packet (a 2-byte
//                       big-endian hex value), including 4 trailing bytes
//                       of CRC, but excluding itself and the flag field.
//                       See below for special values
//             NNNNNNNN  is a 4-byte flag field with the following meanings:
//                       FFFFFFFF  a packet has been dropped (?); in this case
//                                 the length field appears to be always 4000
//                       00000010  there are more packets currently available
//                                 in SCSI/Link memory
//                       00000000  this is the last packet
//             XX XX ... is the actual packet
//             CCCCCCCC  is the CRC
//
//   Notes:
//    - When all packets have been retrieved successfully, a length field
//      of 0000 is returned; however, if a packet has been dropped, the
//      SCSI/Link will instead return a non-zero length field with a flag
//      of FFFFFFFF when there are no more packets available.  This behaviour
//      seems to continue until a disable/enable sequence has been issued.
//    - The SCSI/Link apparently has about 6KB buffer space for packets.
//
//---------------------------------------------------------------------------
int DaynaPort::Read(cdb_t cdb, vector<uint8_t> &buf, uint64_t)
{
    const auto response = (scsi_resp_read_t*)buf.data();

    const int requested_length = cdb[4];

    LogTrace(fmt::format("Read maximum length: {}", requested_length));

    // At startup the host may send a READ(6) command with a sector count of 1 to read the root sector.
    // We should respond by going into the status mode with a code of 0x02.
    if (requested_length == 1) {
        return 0;
    }

    // The first 2 bytes are reserved for the length of the packet
    // The next 4 bytes are reserved for a flag field
    const int rx_packet_size = tap.Receive(&buf[DAYNAPORT_READ_HEADER_SZ]);

    // If we didn't receive anything, return size of 0
    if (rx_packet_size <= 0) {
        LogTrace("No packet received");
        response->length = 0;
        response->flags = read_data_flags_t::e_no_more_data;
        return DAYNAPORT_READ_HEADER_SZ;
    }

    byte_read_count += rx_packet_size;

    int size = rx_packet_size;
    if (size < 128) {
        // A frame must have at least 64 bytes for the Atari driver, see https://github.com/PiSCSI/piscsi/issues/619,
        // but also works with 128 bytes.
        // The NetBSD driver requires at least 128 bytes, see https://github.com/PiSCSI/piscsi/issues/1098.
        // The Mac driver is also fine with 128 bytes.
        // Note that this work-around breaks the checksum. As currently there are no known drivers
        // that care for the checksum it was decided to accept the broken checksum.
        // If a driver should pop up that breaks because of this, the work-around has to be re-evaluated.
        size = 128;
    }

    SetInt16(buf, 0, size);
    SetInt32(buf, 2, tap.HasPendingPackets() ? 0x10 : 0x00);

    // Return the packet size + 2 for the length + 4 for the flag field
    // The CRC was already appended by the ctapdriver
    return size + DAYNAPORT_READ_HEADER_SZ;
}

//---------------------------------------------------------------------------
//
//  Write
//
//   Command:  0a 00 00 LL LL XX (LLLL is data length, XX = 80 or 00)
//   Function: Write a packet at a time to the device (standard SCSI Write)
//   Type:     Output; the format of the data to be sent depends on the value
//             of XX, as follows:
//              - if XX = 00, LLLL is the packet length, and the data to be sent
//                must be an image of the data packet
//              - if XX = 80, LLLL is the packet length + 8, and the data to be
//                sent is:
//                  PP PP 00 00 XX XX XX ... 00 00 00 00
//                where:
//                  PPPP      is the actual (2-byte big-endian) packet length
//               XX XX ... is the actual packet
//
//---------------------------------------------------------------------------
bool DaynaPort::Write(cdb_t cdb, span<const uint8_t> buf)
{
    if (const int data_format = cdb[5]; data_format == 0x00) {
        const int data_length = GetInt16(cdb, 3);
        tap.Send(buf.data(), data_length);
        byte_write_count += data_length;
        LogTrace(fmt::format("Transmitted {} byte(s) (00 format)", data_length));
    }
    else if (data_format == 0x80) {
        // The data length is specified in the first 2 bytes of the payload
        const int data_length = buf[1] + ((static_cast<int>(buf[0]) & 0xff) << 8);
        tap.Send(&(buf.data()[4]), data_length);
        byte_write_count += data_length;
        LogTrace(fmt::format("Transmitted {} byte(s) (80 format)", data_length));
    }
    else {
        LogWarn(fmt::format("Unknown data format: ${:02x}", data_format));
    }

    GetController()->SetBlocks(0);

    return true;
}

//---------------------------------------------------------------------------
//
//	RetrieveStats
//
//   Command:  09 00 00 00 12 00
//   Function: Retrieve MAC address and device statistics
//   Type:     Input; returns 18 (decimal) bytes of data as follows:
//              - bytes 0-5:  the current hardware ethernet (MAC) address
//              - bytes 6-17: three long word (4-byte) counters (little-endian).
//   Notes:    The contents of the three longs are typically zero, and their
//             usage is unclear; they are suspected to be:
//              - long #1: frame alignment errors
//              - long #2: CRC errors
//              - long #3: frames lost
//
//---------------------------------------------------------------------------
int DaynaPort::RetrieveStats(cdb_t cdb, vector<uint8_t> &buf) const
{
    memcpy(buf.data(), &m_scsi_link_stats, sizeof(m_scsi_link_stats));

    // Take the last 3 MAC address bytes from the bridge's MAC address, so that several DaynaPort emulations
    // on different Pis in the same network do not have identical MAC addresses.
    if (const auto &mac = GetMacAddress(CTapDriver::GetBridgeName()); mac.size() >= 6) {
        buf.data()[3] = mac[3];
        buf.data()[4] = mac[4];
        buf.data()[5] = mac[5];
    }

    LogDebug(fmt::format("The DaynaPort MAC address is {0:02x}:{1:02x}:{2:02x}:{3:02x}:{4:02x}:{5:02x}",
        buf.data()[0], buf.data()[1], buf.data()[2], buf.data()[3], buf.data()[4], buf.data()[5]));

    return static_cast<int>(min(sizeof(m_scsi_link_stats), static_cast<size_t>(GetInt16(cdb, 3))));
}

void DaynaPort::TestUnitReady()
{
    // Always successful
    EnterStatusPhase();
}

void DaynaPort::Read6()
{
    const uint32_t record = GetInt24(GetController()->GetCmd(), 1) & 0x1fffff;
    GetController()->SetBlocks(1);

    // If any commands have a bogus control value, they were probably not
    // generated by the DaynaPort driver so ignore them
    if (GetController()->GetCmdByte(5) != 0xc0 && GetController()->GetCmdByte(5) != 0x80) {
        LogTrace("Control value: " + to_string(GetController()->GetCmdByte(5)));
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    LogTrace(fmt::format("READ(6) command, record: ${:02x}", record));

    const int length = Read(GetController()->GetCmd(), GetController()->GetBuffer(), record);
    LogTrace(fmt::format("Length is {}", GetController()->GetLength()));
    GetController()->SetLength(length);

    // Set next block
    GetController()->SetNext(record + 1);

    EnterDataInPhase();
}

void DaynaPort::Write6() const
{
    const int data_format = GetController()->GetCmdByte(5);

    int length = 0;
    if (data_format == 0x00) {
        length = GetInt16(GetController()->GetCmd(), 3);
    }
    else if (data_format == 0x80) {
        length = GetInt16(GetController()->GetCmd(), 3) + 8;
    }

    if (length <= 0) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    LogTrace(fmt::format("Length: {0}, format: ${1:02x}", GetController()->GetLength(), data_format));

    GetController()->SetLength(length);

    // Set next block
    GetController()->SetBlocks(1);
    GetController()->SetNext(1);

    EnterDataOutPhase();
}

void DaynaPort::RetrieveStatistics() const
{
    GetController()->SetLength(RetrieveStats(GetController()->GetCmd(), GetController()->GetBuffer()));

    // Set next block
    GetController()->SetBlocks(1);
    GetController()->SetNext(1);

    EnterDataInPhase();
}

//---------------------------------------------------------------------------
//
//	Set interface mode/Set MAC address
//
//   Set Interface Mode (0c)
//   -----------------------
//   Command:  0c 00 00 00 FF 80 (FF = 08 or 04)
//   Function: Allow interface to receive broadcast messages (FF = 04); the
//             function of (FF = 08) is currently unknown.
//   Type:     No data transferred
//   Notes:    This command is accepted by firmware 1.4a & 2.0f, but has no
//             effect on 2.0f, which is always capable of receiving broadcast
//             messages.  In 1.4a, once broadcast mode is set, it remains set
//             until the interface is disabled.
//
//   Set MAC Address (0c)
//   --------------------
//   Command:  0c 00 00 00 FF 40 (FF = 08 or 04)
//   Function: Set MAC address
//   Type:     Output; overrides built-in MAC address with user-specified
//             6-byte value
//   Notes:    This command is intended primarily for debugging/test purposes.
//             Disabling the interface resets the MAC address to the built-in
//             value.
//
//---------------------------------------------------------------------------
void DaynaPort::SetInterfaceMode() const
{
    switch (GetController()->GetCmdByte(5)) {
    case CMD_SCSILINK_SETMODE:
        // Not implemented, do nothing
        EnterStatusPhase();
        break;

    case CMD_SCSILINK_SETMAC:
        // Currently the MAC address passed is ignored
        GetController()->SetLength(6);
        EnterDataOutPhase();
        break;

    default:
        LogWarn(fmt::format("Unknown SetInterfaceMode mode: ${:02x}", GetController()->GetCmdByte(5)));
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
        break;
    }
}

void DaynaPort::SetMcastAddr() const
{
    const int length = GetController()->GetCmdByte(4);
    if (!length) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    // Currently the multicast address passed is ignored
    GetController()->SetLength(length);
    EnterDataOutPhase();
}

//---------------------------------------------------------------------------
//
//	Enable or Disable the interface
//
//  Command:  0e 00 00 00 00 XX (XX = 80 or 00)
//  Function: Enable (80) / disable (00) Ethernet interface
//  Type:     No data transferred
//  Notes:    After issuing an Enable, the initiator should avoid sending
//            any subsequent commands to the device for approximately 0.5
//            seconds
//
//---------------------------------------------------------------------------
void DaynaPort::EnableInterface()
{
    if (GetController()->GetCmdByte(5) & 0x80) {
        if (const string error = tap.IpLink(true); !error.empty()) {
            LogWarn("Unable to enable the DaynaPort Interface: " + error);
            throw scsi_exception(sense_key::aborted_command);
        }

        tap.Flush();

        // The MacOS DaynaPort driver needs to have a delay after the size/flags field of the read response.
        // The NetBSD drivers for the Mac fail when there is a delay.
        // The Atari drivers (STiNG and MiNT) work with and without a delay.
        // In order to work with all drivers the delay depends on the last INQUIRY received. A peculiarity of
        // the MacOS DaynaPort helps to identify which driver is being used and which delay is the working one.
        if (macos_seen) {
            macos_seen = false;
            SetDelayAfterBytes(DAYNAPORT_READ_HEADER_SZ);
            LogDebug("The DaynaPort interface has been enabled for MacOS");
        }
        else {
            SetDelayAfterBytes(Bus::SEND_NO_DELAY);
            LogDebug("The DaynaPort interface has been enabled");
        }
    }
    else {
        if (const string error = tap.IpLink(false); !error.empty()) {
            LogWarn("Unable to disable the DaynaPort Interface: " + error);
            throw scsi_exception(sense_key::aborted_command);
        }

        LogDebug("The DaynaPort interface has been disabled");
    }

    EnterStatusPhase();
}

vector<PbStatistics> DaynaPort::GetStatistics() const
{
    vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

    PbStatistics s;
    s.set_id(GetId());
    s.set_unit(GetLun());

    s.set_category(PbStatisticsCategory::CATEGORY_INFO);

    s.set_key(BYTE_READ_COUNT);
    s.set_value(byte_read_count);
    statistics.push_back(s);

    s.set_key(BYTE_WRITE_COUNT);
    s.set_value(byte_write_count);
    statistics.push_back(s);

    return statistics;
}
