//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <filesystem>
// TODO Try to get rid of this dependency
#include "shared_protobuf/command_context.h"

using namespace std;
using namespace filesystem;
using namespace s2p_interface;

class S2pImage
{

public:

    S2pImage();
    ~S2pImage() = default;

    void SetDepth(int d)
    {
        depth = d;
    }
    int GetDepth() const
    {
        return depth;
    }
    string GetDefaultFolder() const
    {
        return default_folder;
    }
    string SetDefaultFolder(string_view);
    bool CreateImage(const CommandContext&) const;
    bool DeleteImage(const CommandContext&) const;
    bool RenameImage(const CommandContext&) const;
    bool CopyImage(const CommandContext&) const;
    bool SetImagePermissions(const CommandContext&) const;

private:

    bool CheckDepth(string_view) const;
    string GetFullName(const string &filename) const
    {
        return default_folder + "/" + filename;
    }
    bool CreateImageFolder(const CommandContext&, string_view) const;
    static bool IsReservedFile(const CommandContext&, const string&, const string&);
    bool ValidateParams(const CommandContext&, const string&, string&, string&) const;

    static bool IsValidSrcFilename(string_view);
    static bool IsValidDstFilename(string_view);
    static bool ChangeOwner(const CommandContext&, const path&, bool);
    static string GetHomeDir();
    static pair<int, int> GetUidAndGid();

    // ~/images is the default folder for device image files, for the root user it is /home/pi/images
    string default_folder;

    int depth = 1;
};
