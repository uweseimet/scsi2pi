//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "base/interfaces/scsi_stream_commands.h"
#include "storage_device.h"

using namespace std;

class Tape : public StorageDevice, public ScsiStreamCommands
{

public:

    explicit Tape(int);
    ~Tape() override = default;

    bool SetUp() override;
    void CleanUp() override;

    bool Eject(bool) override;

    int WriteData(span<const uint8_t>, scsi_command) override;

    int ReadData(span<uint8_t>) override;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;

    vector<PbStatistics> GetStatistics() const override;

protected:

    void ValidateFile() override;

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    static constexpr const char *MAGIC = "SCTP";

    enum object_type : uint8_t
    {
        BLOCK = 0b000,
        FILEMARK = 0b001,
        END_OF_DATA = 0b011
    };

    // The meta data for each object, with the object type and payload size
    using meta_data_t = struct _meta_data_t
    {
        array<uint8_t, 4> magic;
        Tape::object_type type;
        uint8_t reserved;
        // Big-endian 16-bit integer
        array<uint8_t, 2> size;
    };

    // Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)

    void Read6() override;
    void Write6() override;
    void Erase6() override;
    void ReadBlockLimits() override;
    void Rewind() override;
    void Space6() override;
    void WriteFilemarks6() override;
    void Locate10()
    {
        Locate(false);
    }
    void Locate16()
    {
        Locate(true);
    }
    void ReadPosition() const;

    void Locate(bool);

    void WriteMetaData(Tape::object_type, uint32_t);
    uint32_t FindNextObject(Tape::object_type, int64_t);

    int GetByteCount() const;

    void AddModeBlockDescriptor(map<int, vector<byte>>&) const;
    void AddMediumPartitionPage(map<int, vector<byte>>&) const;
    void AddDeviceConfigurationPage(map<int, vector<byte>>&) const;

    fstream file;

    uint64_t position = 0;

    uint64_t block_location = 0;

    int byte_count = 0;

    off_t filesize = 0;

    bool tar_mode = false;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;

    static constexpr const char *READ_ERROR_COUNT = "read_error_count";
    static constexpr const char *WRITE_ERROR_COUNT = "write_error_count";
};
