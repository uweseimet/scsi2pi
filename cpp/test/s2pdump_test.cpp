//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022 akuker
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "s2pdump/s2pdump_core.h"
#include "test_shared.h"

using namespace std;
using namespace filesystem;
using namespace testing;

TEST(S2pDumpTest, GeneratePropertiesFile)
{
    // Basic test
    auto filename = CreateTempFile(0);
    S2pDump::inquiry_info_t test_data = { .vendor = "SCSI2Pi", .product = "TEST PRODUCT", .revision = "REV1",
        .sector_size = 1000, .capacity = 100 };
    test_data.GeneratePropertiesFile(filename);

    string expected_str =
        R"({
    "vendor": "SCSI2Pi",
    "product": "TEST PRODUCT",
    "revision": "REV1",
    "block_size": "1000"
}
)";
    EXPECT_EQ(expected_str, ReadTempFileToString(filename));
    DeleteTempFile(filename);

    // Long string test
    filename = CreateTempFile(0);
    test_data = {.vendor = "01234567",
        .product = "0123456789ABCDEF",
        .revision = "0123",
        .sector_size = UINT32_MAX,
        .capacity = UINT64_MAX};
    test_data.GeneratePropertiesFile(filename);

    expected_str =
        R"({
    "vendor": "01234567",
    "product": "0123456789ABCDEF",
    "revision": "0123",
    "block_size": "4294967295"
}
)";
    EXPECT_EQ(expected_str, ReadTempFileToString(filename));
    DeleteTempFile(filename);

    // Empty data test
    filename = CreateTempFile(0);
    test_data = {.vendor = "", .product = "", .revision = "", .sector_size = 0, .capacity = 0};
    test_data.GeneratePropertiesFile(filename);

    expected_str = R"({
    "vendor": "",
    "product": "",
    "revision": ""
}
)";
    EXPECT_EQ(expected_str, ReadTempFileToString(filename));
    DeleteTempFile(filename);
}
