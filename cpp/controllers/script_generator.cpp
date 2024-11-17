//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include  <iomanip>
#include "script_generator.h"
#include "buses/bus_factory.h"
#include "shared/s2p_util.h"

using namespace s2p_util;

void ScriptGenerator::AddCdb(int id, int lun, span<int> cdb)
{
    file << dec << "-i " << id << COMPONENT_SEPARATOR << lun << " -c " << hex;

    for (int i = 0; i < BusFactory::Instance().GetCommandBytesCount(static_cast<scsi_command>(cdb[0])); i++) {
        if (i) {
            file << ":";
        }
        file << setfill('0') << setw(2) << cdb[i];
    }

    file << flush;
}

void ScriptGenerator::AddData(span<uint8_t> data)
{
    file << " -d " << hex;

    for (size_t i = 0; i < data.size(); i++) {
        if (i) {
            file << ":";
        }
        file << setfill('0') << setw(2) << static_cast<int>(data[i]);
    }

    file << flush;
}

void ScriptGenerator::WriteEol()
{
    file << '\n' << flush;
}
