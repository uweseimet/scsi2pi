//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "controllers/script_generator.h"
#include "test_shared.h"

using namespace testing;

TEST(ScriptGeneratorTest, AddCdb)
{
    ScriptGenerator generator;

    EXPECT_FALSE(generator.CreateFile(""));

    const string &filename = CreateTempName();
    EXPECT_TRUE(generator.CreateFile(filename));

    vector<int> cdb = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
    generator.AddCdb(1, 2, cdb);
    vector<uint8_t> data = { 0xff, 0xfe, 0xfd, 0xfc };
    generator.AddData(data);
    generator.WriteEol();
    cdb = { 0x1f, 0x01, 0x02, 0x03 };
    generator.AddCdb(3, 31, cdb);
    generator.WriteEol();

    ifstream in(filename);
    string line;

    getline(in, line);
    EXPECT_EQ("-i 1:2 -c 00:01:02:03:04:05 -d ff:fe:fd:fc", line);

    getline(in, line);
    EXPECT_EQ("-i 3:31 -c 1f:01:02:03", line);
}
