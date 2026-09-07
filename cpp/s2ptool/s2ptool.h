//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string>
#include <vector>

using namespace std;

class Runnable;
class S2p;

namespace s2ptool
{

void Usage();

void AddArg(vector<char*>&, const string&);

[[noreturn]] void TerminationHandler(int);

inline unique_ptr<Runnable> runnable = nullptr;

inline shared_ptr<S2p> s2p_instance = nullptr;

}
