//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// The base class for all mass storage devices with image file support
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include "shared/s2p_util.h"
#include "base/primary_device.h"
#include "page_handler.h"

using namespace std;

class StorageDevice : public PrimaryDevice
{

public:

    ~StorageDevice() override = default;

    bool SetUp() override;
    void CleanUp() override;

    void Dispatch(scsi_command) override;

    bool SupportsFile() const override
    {
        return true;
    }

    bool Eject(bool) override;

    virtual void Open() = 0;

    string GetFilename() const
    {
        return filename.string();
    }
    void SetFilename(string_view file)
    {
        filename = filesystem::path(file);
    }
    string GetLastFilename() const
    {
        return last_filename;
    }

    uint64_t GetBlockCount() const
    {
        return blocks;
    }

    uint32_t GetBlockSize() const
    {
        return block_size;
    }
    const auto& GetSupportedBlockSizes() const
    {
        return supported_block_sizes;
    }
    uint32_t GetConfiguredBlockSize() const
    {
        return configured_block_size;
    }
    bool SetConfiguredBlockSize(uint32_t);
    virtual bool ValidateBlockSize(uint32_t) const;

    bool ReserveFile() const;
    void UnreserveFile();

    static bool FileExists(string_view);

    void SetMediumChanged(bool b)
    {
        medium_changed = b;
    }

    static auto GetReservedFiles()
    {
        return reserved_files;
    }
    static void SetReservedFiles(const unordered_map<string, id_set, s2p_util::StringHash, equal_to<>> &r)
    {
        reserved_files = r;
    }
    static id_set GetIdsForReservedFile(const string&);

    vector<PbStatistics> GetStatistics() const override;

protected:

    StorageDevice(PbDeviceType type, scsi_level level, int lun, bool s, bool p, const unordered_set<uint32_t> &sizes)
    : PrimaryDevice(type, level, lun), supported_block_sizes(sizes), supports_mode_select(s), supports_save_parameters(
        p)
    {
    }

    virtual void ValidateFile();

    void CheckWritePreconditions() const;

    bool IsMediumChanged() const
    {
        return medium_changed;
    }

    void SetBlockCount(uint64_t b)
    {
        blocks = b;
    }

    void ModeSelect(cdb_t, span<const uint8_t>, int) override;
    pair<int, int> EvaluateBlockDescriptors(scsi_command, span<const uint8_t>, int) const;
    int VerifyBlockSizeChange(int, bool) const;
    unordered_set<uint32_t> GetBlockSizes() const;
    bool SetBlockSize(uint32_t);

    virtual void ChangeBlockSize(uint32_t);

    off_t GetFileSize() const;

    void UpdateReadCount(uint64_t count)
    {
        block_read_count += count;
    }
    void UpdateWriteCount(uint64_t count)
    {
        block_write_count += count;
    }

    void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

    // Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)

    void StartStopUnit();
    void PreventAllowMediumRemoval();

    bool IsReadOnlyFile() const;

    int ModeSense6(cdb_t, vector<uint8_t>&) const override;
    int ModeSense10(cdb_t, vector<uint8_t>&) const override;

    void AddReadWriteErrorRecoveryPage(map<int, vector<byte>>&) const;
    void AddDisconnectReconnectPage(map<int, vector<byte>>&) const;
    void AddControlModePage(map<int, vector<byte>>&) const;

    unique_ptr<PageHandler> page_handler;

    uint64_t blocks = 0;

    // Block sizes in bytes
    unordered_set<uint32_t> supported_block_sizes;
    uint32_t configured_block_size = 0;
    uint32_t block_size = 0;

    bool supports_mode_select;

    bool supports_save_parameters;

    filesystem::path filename;

    string last_filename;

    bool medium_changed = false;

    uint64_t block_read_count = 0;
    uint64_t block_write_count = 0;

    static constexpr const char *BLOCK_READ_COUNT = "block_read_count";
    static constexpr const char *BLOCK_WRITE_COUNT = "block_write_count";

    // The list of image files in use and the IDs and LUNs using these files
    static inline unordered_map<string, id_set, s2p_util::StringHash, equal_to<>> reserved_files;
};
