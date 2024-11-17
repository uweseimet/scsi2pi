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

enum class simh_marker : uint32_t
{
    erase_gap = 0xffffffe,
    end_of_medium = 0xfffffff
};

using SimhHeader = struct _SimhHeader {
    simh_class cls;
    uint32_t value;
};

int ReadHeader(istream&, SimhHeader&);

bool IsRecord(simh_class);

uint32_t GetPadding(int);

uint32_t FromLittleEndian(span<const uint8_t>);
array<uint8_t, 4> ToLittleEndian(uint32_t);

static const int64_t HEADER_SIZE = static_cast<int64_t>(sizeof(uint32_t));

// "S2P", private marker magic value for tape object types
static const uint32_t PRIVATE_MARKER_MAGIC = 0x53325000;

static const int OVERFLOW_ERROR = -1;
static const int READ_ERROR = -2;
static const int WRITE_ERROR = -3;

}
