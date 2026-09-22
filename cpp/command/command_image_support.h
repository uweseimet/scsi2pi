//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

class CommandContext;

using namespace std;

namespace command_image_support
{

void SetDepth(int);
int GetDepth();

const string& GetImageFolder();
string SetImageFolder(string_view);

bool CreateImage(const CommandContext &context);
bool DeleteImage(const CommandContext &context);
bool RenameImage(const CommandContext &context);
bool CopyImage(const CommandContext &context);
bool SetImagePermissions(const CommandContext &context);

}
