//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "file_util.h"
#include <fcntl.h>
#if __has_include(<sys/ioctl.h>)
#include <sys/ioctl.h>
#endif
#if __has_include(<linux/fs.h>)
#include <linux/fs.h>
#include <sys/stat.h>
#endif
#include <unistd.h>
#include "s2p_exceptions.h"
#include "s2p_util.h"

using namespace filesystem;
using namespace s2p_util;

bool file_util::IsReadOnlyFile(const path &filename)
{
    const auto s = status(filename);
    return exists(s) && (s.permissions() & perms::owner_write) == perms::none;
}

string file_util::GetExtensionLowerCase(const path &filename)
{
    const string ext = ToLower(filename.extension().string());

    // Remove the leading dot
    return ext.empty() ? ext : ext.substr(1);
}

off_t file_util::GetCapacityFromFile(const path &filename)
{
    string error_message;
    const auto f = filename.string();

#if __has_include(<linux/fs.h>)
    if (struct stat st; !stat(f.c_str(), &st) && S_ISBLK(st.st_mode)) {
        const int fd = open(f.c_str(), O_RDONLY);
        int error = errno;
        if (fd != -1) {
            uint64_t size = 0;
            const int ret = ioctl(fd, BLKGETSIZE64, &size);
            error = errno;
            close(fd);

            if (ret != -1) {
                return static_cast<off_t>(size);
            }
        }

        error_message = system_error(error, generic_category()).what();
    }
    else
#endif

    {
        error_code error;
        const off_t size = file_size(filename, error);
        if (!error) {
            return size;
        }

        error_message = error.message();
    }

    throw IoException("Can't get file size of '{}': {}", f, error_message);
}
