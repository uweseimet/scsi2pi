//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "bus.h"

using namespace std;
using namespace scsi_defs;

int Bus::GetCommandByteCount(int opcode)
{
    const auto &mapping = COMMAND_MAPPING.find(static_cast<scsi_command>(opcode));

    return mapping != COMMAND_MAPPING.end() ? mapping->second.first : 0;
}

phase_t Bus::GetPhase()
{
    Acquire();

    if (GetSEL()) {
        return phase_t::selection;
    }

    if (!GetBSY()) {
        return phase_t::busfree;
    }

    // Get target phase from bus signal line
    int mci = GetMSG() ? 0b100 : 0b000;
    mci |= GetCD() ? 0b010 : 0b000;
    mci |= GetIO() ? 0b001 : 0b000;
    return GetPhase(mci);
}

string Bus::GetPhaseName(phase_t phase)
{
    assert(phase_names.find(phase) != phase_names.end());
    return phase_names.at(phase);
}

// Phase Table
// Reference Table 8: https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-06.html
// This determines the phase based upon the Msg, C/D and I/O signals.
//
// |MSG|C/D|I/O| Phase
// | 0 | 0 | 0 | DATA OUT
// | 0 | 0 | 1 | DATA IN
// | 0 | 1 | 0 | COMMAND
// | 0 | 1 | 1 | STATUS
// | 1 | 0 | 0 | RESERVED
// | 1 | 0 | 1 | RESERVED
// | 1 | 1 | 0 | MESSAGE OUT
// | 1 | 1 | 1 | MESSAGE IN
//
const array<phase_t, 8> Bus::phases = {
    phase_t::dataout,
    phase_t::datain,
    phase_t::command,
    phase_t::status,
    phase_t::reserved,
    phase_t::reserved,
    phase_t::msgout,
    phase_t::msgin
};

const unordered_map<phase_t, string> Bus::phase_names = {
    { phase_t::busfree, "BUS FREE" },
    { phase_t::arbitration, "ARBITRATION" },
    { phase_t::selection, "SELECTION" },
    { phase_t::reselection, "RESELECTION" },
    { phase_t::command, "COMMAND" },
    { phase_t::datain, "DATA IN" },
    { phase_t::dataout, "DATA OUT" },
    { phase_t::status, "STATUS" },
    { phase_t::msgin, "MESSAGE IN" },
    { phase_t::msgout, "MESSAGE OUT" },
    { phase_t::reserved, "reserved" }
};
