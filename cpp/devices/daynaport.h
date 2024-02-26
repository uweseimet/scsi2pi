//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2020 akuker
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2023-2024 Uwe Seimet
//
// This design is derived from the SLINKCMD.TXT file, as well as David Kuder's
// Tiny SCSI Emulator
//    - SLINKCMD: http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/SLINKCMD.TXT
//    - Tiny SCSI : https://hackaday.io/project/18974-tiny-scsi-emulator
//
// Special thanks to @PotatoFi for loaning me his Farallon EtherMac for
// this development. (Farallon's EtherMac is a re-branded DaynaPort
// SCSI/Link-T).
//
// Note: This requires a DaynaPort SCSI Link driver. It has successfully been tested with MacOS and the Atari.
//
//---------------------------------------------------------------------------

#pragma once

#ifndef __NetBSD__
#include <net/ethernet.h>
#endif
#include <unordered_map>
#include <array>
#include "base/interfaces/scsi_communications_commands.h"
#include "base/primary_device.h"
#include "tap_driver.h"

class DaynaPort : public PrimaryDevice, private ScsiCommunicationsCommands
{
    uint64_t byte_read_count = 0;
    uint64_t byte_write_count = 0;

    inline static const string BYTE_READ_COUNT = "byte_read_count";
    inline static const string BYTE_WRITE_COUNT = "byte_write_count";

public:

    explicit DaynaPort(int);
    ~DaynaPort() override = default;

    bool Init(const param_map&) override;
    void CleanUp() override;

    param_map GetDefaultParams() const override
    {
        return tap.GetDefaultParams();
    }

    vector<uint8_t> InquiryInternal() const override;
    int Read(cdb_t, vector<uint8_t>&, uint64_t);
    int WriteData(span<const uint8_t>, scsi_command) override;

    int RetrieveStats(cdb_t, vector<uint8_t>&) const;

    void TestUnitReady() override;
    void GetMessage6() override;
    void SendMessage6() const override;
    void RetrieveStatistics() const;
    void SetInterfaceMode() const;
    void SetMcastAddr() const;
    void EnableInterface() const;

    vector<PbStatistics> GetStatistics() const override;

    static constexpr int CMD_SCSILINK_STATS = 0x09;
    static constexpr int CMD_SCSILINK_ENABLE = 0x0e;
    static constexpr int CMD_SCSILINK_SET = 0x0c;
    static constexpr int CMD_SCSILINK_SETMAC = 0x40;
    static constexpr int CMD_SCSILINK_SETMODE = 0x80;

    // The READ response has a header which consists of:
    //   2 bytes - payload size
    //   4 bytes - status flags
    static constexpr uint32_t DAYNAPORT_READ_HEADER_SZ = 2 + 4;

private:

    enum class read_data_flags_t : uint32_t
    {
        e_no_more_data = 0x00000000,
        e_more_data_available = 0x00000001,
        e_dropped_packets = 0xFFFFFFFF,
    };

    using scsi_resp_read_t = struct __attribute__((packed)) {
        uint32_t length;
        read_data_flags_t flags;
        byte pad;
        array<byte, ETH_FRAME_LEN + sizeof(uint32_t)> data; // Frame length + 4 byte CRC
    };

    using scsi_resp_link_stats_t = struct __attribute__((packed)) {
        array<byte, 6> mac_address;
        uint32_t frame_alignment_errors;
        uint32_t crc_errors;
        uint32_t frames_lost;
    };

    scsi_resp_link_stats_t m_scsi_link_stats = {
        // The last 3 bytes of this MAC address are replaced by those of the bridge interface
        .mac_address = { byte { 0x00 }, byte { 0x80 }, byte { 0x19 }, byte { 0x10 }, byte { 0x98 }, byte { 0xe3 } },
        .frame_alignment_errors = 0,
        .crc_errors = 0,
        .frames_lost = 0,
    };

    TapDriver tap;

    bool tap_enabled = false;
};
