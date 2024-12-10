//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <fstream>
#include <span>

using namespace std;

namespace simh_util
{

enum class simh_class
{
    tape_mark_good_data_record = 0,
    private_data_record_1 = 1,
    private_data_record_2 = 2,
    private_data_record_3 = 3,
    private_data_record_4 = 4,
    private_data_record_5 = 5,
    private_data_record_6 = 6,
    private_marker = 7,
    bad_data_record = 8,
    reserved_data_record_1 = 9,
    reserved_data_record_2 = 10,
    reserved_data_record_3 = 11,
    reserved_data_record_4 = 12,
    reserved_data_record_5 = 13,
    tape_description_data_record = 14,
    reserved_marker = 15,
};

enum class simh_marker
{
    erase_gap = 0xffffffe,
    end_of_medium = 0xfffffff
};

using SimhMetaData = struct _SimhMetaData {
    _SimhMetaData(simh_class c =simh_class::bad_data_record, uint32_t v = 0) : cls(c), value(v) {}

    simh_class cls;
    uint32_t value;
};

bool ReadMetaData(istream&, SimhMetaData&);

bool IsRecord(const SimhMetaData&);

uint32_t Pad(int);

void WriteFilemark(ostream&);
void WriteGoodData(ostream&, span<const uint8_t>, int);

SimhMetaData FromLittleEndian(span<const uint8_t>);
array<uint8_t, 4> ToLittleEndian(const SimhMetaData&);

static const int64_t META_DATA_SIZE = static_cast<int64_t>(sizeof(uint32_t));

// "S2P", private marker magic value for tape object types, the SCSI2Pi type is coded in the low nibble of the LSB
static const uint32_t PRIVATE_MARKER_MAGIC = 0x00533250;

static const int OVERFLOW_ERROR = -1;
static const int READ_ERROR = -2;
static const int WRITE_ERROR = -3;

}
