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

    const string &filename = CreateTempFile();
    EXPECT_TRUE(generator.CreateFile(filename));

    auto cdb = CreateCdb(scsi_command::test_unit_ready, "01:02:03:04:05");
    generator.AddCdb(1, 2, cdb);
    vector<uint8_t> data = { 0xff, 0xfe, 0xfd, 0xfc };
    generator.AddData(data);
    generator.WriteEol();
    cdb = CreateCdb(static_cast<scsi_command>(0x1f), "01:02:03");
    assert(!cdb.empty());
    generator.AddCdb(3, 31, cdb);
    generator.WriteEol();

    generator.AddCdb(3, 31, cdb);
    data.clear();
    for (uint8_t i = 0; i < 34; i++) {
        data.push_back(i);
    }
    generator.AddData(data);
    generator.WriteEol();

    ifstream in(filename);
    string line;

    getline(in, line);
    EXPECT_EQ("-i 1:2 -c 00:01:02:03:04:05 -d ff:fe:fd:fc", line);

    getline(in, line);
    EXPECT_EQ("-i 3:31 -c 1f:01:02:03", line);

    getline(in, line);
    EXPECT_EQ("-i 3:31 -c 1f:01:02:03 -d 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f\\", line);

    getline(in, line);
    EXPECT_EQ("10:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:1e:1f\\", line);
    getline(in, line);
    EXPECT_EQ("20:21", line);
    getline(in, line);

    EXPECT_TRUE(line.empty());
}
