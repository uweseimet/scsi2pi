//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "shared/sg_adapter.h"

TEST(SgAdapterTest, Init)
{
    SgAdapter adapter;

    EXPECT_NE("", adapter.Init("/dev/null"));
    EXPECT_NE("", adapter.Init("/dev/sg12345"));
}
