//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include "shared/shared_exceptions.h"
#include "storage_device.h"

using namespace std;
using namespace filesystem;

StorageDevice::StorageDevice(PbDeviceType type, int lun, bool supports_mode_pages)
: ModePageDevice(type, lun, supports_mode_pages)
{
    SupportsFile(true);
    SetStoppable(true);
}

void StorageDevice::CleanUp()
{
    UnreserveFile();

    ModePageDevice::CleanUp();
}

void StorageDevice::SetFilename(string_view f)
{
    filename = filesystem::path(f);

    // Permanently write-protected
    SetReadOnly(IsReadOnlyFile());

    SetProtectable(!IsReadOnlyFile());

    if (IsReadOnlyFile()) {
        SetProtected(false);
    }
}

void StorageDevice::ValidateFile()
{
    if (!blocks) {
        throw io_exception(string(GetTypeString()) + " device has 0 blocks");
    }

    if (!exists(filename)) {
        throw file_not_found_exception(
            "Image file '" + filename.string() + "' for " + GetTypeString() + " device does not exist");
    }

    if (GetFileSize() > 2LL * 1024 * 1024 * 1024 * 1024) {
        throw io_exception("Image files > 2 TiB are not supported");
    }

    // TODO Check for duplicate handling of these properties (-> S2pExecutor)
    if (IsReadOnlyFile()) {
        // Permanently write-protected
        SetReadOnly(true);
        SetProtectable(false);
        SetProtected(false);
    }

    SetStopped(false);
    SetRemoved(false);
    SetLocked(false);
    SetReady(true);
}

void StorageDevice::ReserveFile() const
{
    assert(!filename.empty());
    assert(!reserved_files.contains(filename.string()));

    reserved_files[filename.string()] = { GetId(), GetLun() };
}

void StorageDevice::UnreserveFile()
{
    reserved_files.erase(filename.string());

    filename.clear();
}

id_set StorageDevice::GetIdsForReservedFile(const string &file)
{
    if (const auto &it = reserved_files.find(file); it != reserved_files.end()) {
        return {it->second.first, it->second.second};
    }

    return {-1, -1};
}

bool StorageDevice::FileExists(string_view file)
{
    return exists(path(file));
}

bool StorageDevice::IsReadOnlyFile() const
{
    return access(filename.c_str(), W_OK);
}

off_t StorageDevice::GetFileSize() const
{
    try {
        return file_size(filename);
    }
    catch (const filesystem_error &e) {
        throw io_exception("Can't get size of '" + filename.string() + "': " + e.what());
    }
}
