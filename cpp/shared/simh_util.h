//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <fstream>
#include <span>

using namespace std;

namespace simh_util
{

enum class SimhClass
{
    TAPE_MARK_GOOD_DATA_RECORD = 0,
    PRIVATE_DATA_RECORD_1 = 1,
    PRIVATE_DATA_RECORD_2 = 2,
    PRIVATE_DATA_RECORD_3 = 3,
    PRIVATE_DATA_RECORD_4 = 4,
    PRIVATE_DATA_RECORD_5 = 5,
    PRIVATE_DATA_RECORD_6 = 6,
    PRIVATE_MARKER = 7,
    BAD_DATA_RECORD = 8,
    RESERVED_DATA_RECORD_1 = 9,
    RESERVED_DATA_RECORD_2 = 10,
    RESERVED_DATA_RECORD_3 = 11,
    RESERVED_DATA_RECORD_4 = 12,
    RESERVED_DATA_RECORD_5 = 13,
    TAPE_DESCRIPTION_DATA_RECORD = 14,
    RESERVERD_MARKER = 15,
};

enum class SimhMarker
{
    ERASE_GAP = 0xffffffe,
    END_OF_MEDIUM = 0xfffffff
};

struct SimhMetaData
{
    SimhClass cls;
    uint32_t value;
};

bool ReadMetaData(istream&, SimhMetaData&);

bool IsRecord(const SimhMetaData&);

uint32_t Pad(int);

bool WriteFilemark(ostream&);
bool WriteGoodData(ostream&, span<const uint8_t>, int);

SimhMetaData FromLittleEndian(span<const uint8_t>);
array<uint8_t, 4> ToLittleEndian(const SimhMetaData&);

static const int64_t META_DATA_SIZE = static_cast<int64_t>(sizeof(uint32_t));

// "S2P", private marker magic value for tape object types, the SCSI2Pi type is coded in the low nibble of the LSB
static const uint32_t PRIVATE_MARKER_MAGIC = 0x00533250;

}
