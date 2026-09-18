//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "script_generator.h"
#include <cassert>
#include <iomanip>
#include <spdlog/fmt/fmt.h>
#include "shared/command_meta_data.h"
#include "shared/s2p_util.h"

using namespace s2p_util;

string ScriptGenerator::CreateFile(const string &filename)
{
    file.open(filename);
    if (!file.good()) {
        return fmt::format("Can't create script file '{}': {}", filename,
            system_error(errno, generic_category()).what());
    }

    return "";
}

bool ScriptGenerator::AddCdb(int id, int lun, cdb_t cdb)
{
    assert(!cdb.empty());

    file << fmt::format("\n-i {}{}{} -c ", id, COMPONENT_SEPARATOR, lun);

    int count = CommandMetaData::GetInstance().GetByteCount(static_cast<ScsiCommand>(cdb[0]));
    // In case of an unknown command add the complete CDB
    if (!count) {
        count = static_cast<int>(cdb.size());
    }
    count = min(count, static_cast<int>(cdb.size()));

    for (int i = 0; i < count; ++i) {
        if (i) {
            file << ':';
        }
        file << fmt::format("{:02x}", cdb[i]);
    }

    file << flush;

    return file.good();
}

bool ScriptGenerator::AddData(span<const uint8_t> data)
{
    assert(!data.empty());

    file << " -d " << hex;

    for (size_t i = 0; i < data.size(); ++i) {
        if (i) {
            file << (i % 16 == 0 ? "\\\n" : ":");
        }
        file << setfill('0') << setw(2) << static_cast<int>(data[i]);
    }

    file << flush;

    return file.good();
}
