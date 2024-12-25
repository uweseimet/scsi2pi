//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <fstream>
#include "shared/simh_util.h"
#include "storage_device.h"

using namespace std;
using namespace simh_util;

class Tape : public StorageDevice
{

public:

    explicit Tape(int);
    ~Tape() override = default;

    string SetUp() override;
    void CleanUp() override;

    param_map GetDefaultParams() const override;

    bool Eject(bool) override;

    int WriteData(cdb_t, data_out_t, int, int) override;

    int ReadData(data_in_t) override;

    void Open() override;

    vector<uint8_t> InquiryInternal() const override;

    bool ValidateBlockSize(uint32_t) const override;

    uint32_t GetBlockSizeForDescriptor(bool changeable) const override
    {
        return changeable ? 0x00ffffff : block_size_for_descriptor;
    }
    uint64_t GetBlockCountForDescriptor() const override
    {
        return 0;
    }

    vector<PbStatistics> GetStatistics() const override;

protected:

    void ValidateFile() override;

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

    uint32_t VerifyBlockSizeChange(uint32_t, bool) override;

private:

    enum class object_type
    {
        block = 0b000,
        filemark = 0b001,
        end_of_data = 0b011,
    };

    // Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)

    void Read(bool);
    void Write(bool);
    void Erase6();
    void ReadBlockLimits() const;
    void Rewind();
    void Space6();
    void WriteFilemarks(bool);
    void FormatMedium();
    void ReadPosition() const;
    bool Locate(bool);

    void WriteMetaData(Tape::object_type, uint32_t = 0);
    SimhMetaData FindNextObject(Tape::object_type, int32_t, bool);
    bool ReadNextMetaData(SimhMetaData&, bool);
    bool FindObject(uint32_t);

    [[noreturn]] void RaiseBeginningOfPartition(int32_t);
    [[noreturn]] void RaiseEndOfPartition(int32_t);
    [[noreturn]] void RaiseEndOfData(Tape::object_type, int32_t);
    [[noreturn]] void RaiseFilemark(int32_t, bool);
    [[noreturn]] void RaiseReadError(const SimhMetaData&);

    uint32_t GetByteCount();

    void AddModeBlockDescriptor(map<int, vector<byte>>&) const;
    void AddMediumPartitionPage(map<int, vector<byte>>&, bool) const;
    void AddDataCompressionPage(map<int, vector<byte>>&) const;
    void AddDeviceConfigurationPage(map<int, vector<byte>>&, bool) const;

    void Erase();

    void ResetPositions();

    pair<Tape::object_type, int> ReadSimhMetaData(SimhMetaData&, int32_t, bool);
    int WriteSimhMetaData(simh_class, uint32_t);

    uint32_t CheckBlockLength();

    bool IsAtRecordBoundary();

    void CheckForOverflow(int64_t);
    void CheckForReadError();
    void CheckForWriteError();

    static int32_t GetSignedInt24(cdb_t, int);

    fstream file;

    SimhMetaData current_meta_data = { };

    int64_t tape_position = 0;

    bool initial = false;

    bool fixed = false;

    uint32_t block_size_for_descriptor = 0;

    int blocks_read = 0;

    uint64_t record_start = 0;
    uint32_t record_length = 0;

    uint64_t object_location = 0;

    uint32_t byte_count = 0;
    uint32_t remaining_count = 0;

    off_t file_size = 0;

    off_t max_file_size = 0;

    bool tar_file = false;

    bool expl = false;

    uint64_t read_error_count = 0;
    uint64_t write_error_count = 0;

    static constexpr const char *APPEND = "append";

    static constexpr const char *READ_ERROR_COUNT = "read_error_count";
    static constexpr const char *WRITE_ERROR_COUNT = "write_error_count";
};
