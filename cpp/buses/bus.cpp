//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus.h"

using namespace std;
using namespace scsi_defs;

int Bus::GetCommandByteCount(uint8_t opcode)
{
    const auto &mapping = command_mapping.find(static_cast<scsi_command>(opcode));

    return mapping != command_mapping.end() ? mapping->second.first : 0;
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
    const auto &it = phase_names.find(phase);
    return it != phase_names.end() ? it->second : "????";
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
    { phase_t::busfree, "busfree" },
    { phase_t::arbitration, "arbitration" },
    { phase_t::selection, "selection" },
    { phase_t::reselection, "reselection" },
    { phase_t::command, "command" },
    { phase_t::datain, "datain" },
    { phase_t::dataout, "dataout" },
    { phase_t::status, "status" },
    { phase_t::msgin, "msgin" },
    { phase_t::msgout, "msgout" },
    { phase_t::reserved, "reserved" }
};
