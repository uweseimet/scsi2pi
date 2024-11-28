//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include <iomanip>
#include "script_generator.h"
#include "buses/bus_factory.h"
#include "shared/s2p_util.h"

using namespace s2p_util;

bool ScriptGenerator::CreateFile(const string &filename)
{
    file.open(filename, ios::out);

    return file.good();
}

void ScriptGenerator::AddCdb(int id, int lun, cdb_t cdb)
{
    assert(!cdb.empty());

    file << dec << "-i " << id << COMPONENT_SEPARATOR << lun << " -c " << hex;

    int count = BusFactory::Instance().GetCommandBytesCount(static_cast<scsi_command>(cdb[0]));
    // In case of an unknown command add all available CDB data
    if (!count) {
        count = static_cast<int>(cdb.size());
    }

    for (int i = 0; i < count; i++) {
        if (i) {
            file << ':';
        }
        file << setfill('0') << setw(2) << cdb[i];
    }

    file << flush;
}

void ScriptGenerator::AddData(span<const uint8_t> data)
{
    assert(!data.empty());

    file << " -d " << hex;

    for (size_t i = 0; i < data.size(); i++) {
        if (i) {
            if (!(i % 16)) {
                file << "\\\n";
            }
            else {
                file << ':';
            }
        }
        file << setfill('0') << setw(2) << static_cast<int>(data[i]);
    }

    file << flush;
}

void ScriptGenerator::WriteEol()
{
    file << '\n' << flush;
}
