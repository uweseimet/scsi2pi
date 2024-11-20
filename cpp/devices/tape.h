//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "base/interfaces/scsi_stream_commands.h"
#include "shared/simh_util.h"
#include "storage_device.h"

using namespace std;
using namespace simh_util;

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

    bool ValidateBlockSize(uint32_t) const override;

    vector<PbStatistics> GetStatistics() const override;

protected:

    void ValidateFile() override;

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    enum class object_type
    {
        block = 0b000,
        filemark = 0b001,
        end_of_data = 0b011,
        // SCSI2Pi-specific
        end_of_partition = -1,
    };

    // Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)

    void Read6() override;
    void Write6() override;
    void Erase6() override;
    void ReadBlockLimits() override;
    void Rewind() override;
    void Space6() override;
    void WriteFilemarks6() override;
    void FormatMedium();
    void ReadPosition() const;
    void Locate(bool);

    void WriteMetaData(Tape::object_type, uint32_t = 0);
    uint32_t FindNextObject(Tape::object_type, int64_t);

    bool MoveBackwards(int64_t);

    uint32_t GetByteCount();

    int VerifyBlockSizeChange(int, bool) const override;

    void AddModeBlockDescriptor(map<int, vector<byte>>&) const;
    void AddMediumPartitionPage(map<int, vector<byte>>&, bool) const;
    void AddDataCompressionPage(map<int, vector<byte>>&) const;
    void AddDeviceConfigurationPage(map<int, vector<byte>>&, bool) const;

    void Erase();

    void ResetPosition();
    void AdjustForSpacing(const SimhMetaData&, bool);
    void AdjustForReading(const SimhMetaData&, bool);
    uint32_t AdjustResult(const SimhMetaData&, bool, object_type);

    pair<Tape::object_type, int> ReadSimhMetaData(SimhMetaData&, bool, bool);
    int WriteSimhMetaData(simh_class, uint32_t);

    void CheckLength(int);

    bool IsAtRecordBoundary() const;

    void CheckForReadError();
    void CheckForWriteError();

    fstream file;

    int64_t position = 0;

    bool fixed = false;

    int blocks_read = 0;

    uint32_t record_length = 0;

    uint64_t block_location = 0;

    uint32_t byte_count = 0;
    uint32_t remaining_count = 0;

    off_t file_size = 0;

    bool tar_file = false;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;

    static constexpr const char *READ_ERROR_COUNT = "read_error_count";
    static constexpr const char *WRITE_ERROR_COUNT = "write_error_count";
};
