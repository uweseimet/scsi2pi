//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <gtest/gtest.h>
#include "s2p/s2p_core.h"
#include "s2pexec/s2pexec_core.h"
#include "test_shared.h"

using namespace s2p_test;

class IntegrationTest : public ::testing::Test
{
public:

    static string ReadFileToString(const string &path)
    {
        ifstream file(path);
        if (!file) {
            return "";
        }

        ostringstream ss;
        ss << file.rdbuf();
        string s = ss.str();

        while (s.ends_with('\n') || s.ends_with('\r')) {
            s.pop_back();
        }

        return s;
    }

    static void AddArg(vector<char*> &args, const string &arg)
    {
        args.emplace_back(strdup(arg.c_str()));
    }

    static void SetUpTestSuite()
    {
        vector<char*> s2p_args;
        AddArg(s2p_args, "s2p");
        AddArg(s2p_args, "-L");
        AddArg(s2p_args, "debug");
        AddArg(s2p_args, "-i");
        AddArg(s2p_args, "0");
        AddArg(s2p_args, "services");

        s2p = make_shared<S2p>();
        s2p_thread = jthread([s2p_args]() mutable {
            s2p->Run(s2p_args);
        });

        // Wait for s2p on the virtual bus up to 1 s
        const auto now = chrono::steady_clock::now();
        while (!s2p->Ready()) {
            if (chrono::steady_clock::now() - now >= chrono::seconds(1)) {
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }

        EXPECT_TRUE(s2p->Ready());
    }

    static void TearDownTestSuite()
    {
        s2p->CleanUp();

        s2p_thread.join();
    }

private:

    inline static shared_ptr<S2p> s2p;

    inline static jthread s2p_thread;
};

string RunTest(const string &command)
{
    const string filename = CreateTempName();
    TestShared::RememberTempFile(filename);

    vector<char*> client_args;
    IntegrationTest::AddArg(client_args, "s2pexec");
    IntegrationTest::AddArg(client_args, "-T");
    IntegrationTest::AddArg(client_args, filename);
    IntegrationTest::AddArg(client_args, "-i");
    IntegrationTest::AddArg(client_args, "0");
    IntegrationTest::AddArg(client_args, "-c");
    IntegrationTest::AddArg(client_args, command);

    make_unique<S2pExec>()->Run(client_args);

    return IntegrationTest::ReadFileToString(filename);
}

TEST_F(IntegrationTest, TestUnitReady)
{
    EXPECT_NO_THROW(RunTest("00:00:00:00:00:00"));
}

TEST_F(IntegrationTest, Inquiry)
{
    const string &expected =
        R"(00000000  03:00:05:02:1f:00:00:08:53:43:53:49:32:50:69:20  '........SCSI2Pi '
00000010  48:6f:73:74:20:53:65:72:76:69:63:65:73:20:20:20  'Host Services   ')";

    const string result = RunTest("12:00:00:00:20:00");

    EXPECT_EQ(expected, result);
}
