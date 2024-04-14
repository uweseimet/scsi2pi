//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <filesystem>

using namespace std;
using namespace filesystem;

class CommandContext;

class CommandImageSupport
{

public:

    static CommandImageSupport& Instance()
    {
        static CommandImageSupport instance; // NOSONAR instance cannot be inlined
        return instance;
    }

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

    CommandImageSupport();

    bool CheckDepth(string_view) const;
    string GetFullName(const string &filename) const
    {
        return default_folder + "/" + filename;
    }
    bool CreateImageFolder(const CommandContext&, string_view) const;
    bool ValidateParams(const CommandContext&, const string&, string&, string&) const;

    static bool IsReservedFile(const CommandContext&, const string&, const string&);
    static bool IsValidSrcFilename(string_view);
    static bool IsValidDstFilename(string_view);
    static bool ChangeOwner(const CommandContext&, const path&, bool);

    int depth = 1;

    string default_folder;
};
