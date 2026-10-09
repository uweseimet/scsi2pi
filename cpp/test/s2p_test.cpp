//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <fcntl.h>
#include <getopt.h>
#include <gtest/gtest.h>
#include "buses/bus_factory.h"
#include "shared/s2p_util.h"
#include "test_shared.h"

static bool ParseBoolFlag(string_view arg, string_view opt, string_view flag_prefix, bool def)
{
    if (arg == opt || arg == flag_prefix || arg == string(opt) + "=true" || arg == string(flag_prefix) + "=true") {
        return true;
    }

    if (arg == string(flag_prefix) + "=false" || arg == string(opt) + "=false") {
        return false;
    }

    return def;
}

static void Usage()
{
    cout << "Usage: s2p_test [options]\n"
        << "  --enable-logging/-l     Enable logging on trace level.\n"
        << "  --integration-tests/-i  Run integration tests instead of unit tests.\n"
        << "  --help/-h               Display this help.\n";
}

int main(int argc, char *argv[])
{
    bool enable_logging = false;
    bool integration_tests = false;

    for (int i = 1; i < argc; ++i) {
        const string_view arg(argv[i]);
        if (arg.rfind("-i", 0) == 0 || arg.rfind("--integration-tests", 0) == 0) {
            integration_tests = ParseBoolFlag(arg, "-i", "--integration-tests", integration_tests);
        } else if (arg.rfind("-l", 0) == 0 || arg.rfind("--enable-logging", 0) == 0) {
            enable_logging = ParseBoolFlag(arg, "-l", "--enable-logging", enable_logging);
        }
        else if (arg.rfind("-h", 0) == 0 || arg.rfind("--help", 0) == 0) {
            cout << s2p_util::Banner("(Test Suite Driver)") << flush;
            Usage();
            return EXIT_SUCCESS;
        }
        else {
            cerr << "Invalid option: '" << arg << "'\n";
            Usage();
            return EXIT_FAILURE;
        }
    }

    spdlog::set_level(enable_logging ? spdlog::level::trace : spdlog::level::off);

    int fd = -1;
    if (!enable_logging) {
        fd = open("/dev/null", O_WRONLY);
        if (fd != -1) {
            dup2(fd, STDERR_FILENO);
        }
    }

    BusFactory::GetInstance().EnableVirtualBus();

    ::testing::InitGoogleTest(&argc, argv);
    if (integration_tests) {
        ::testing::GTEST_FLAG(filter) = "IntegrationTest.*";
        ::testing::GTEST_FLAG(shuffle) = false;
    }
    else {
        ::testing::GTEST_FLAG(filter) = "-IntegrationTest.*";
        ::testing::GTEST_FLAG(shuffle) = true;
    }
    const int result = RUN_ALL_TESTS();

    s2p_test::TestShared::CleanUp();

    if (fd != -1) {
        close(fd);
    }

    return result ? EXIT_FAILURE : EXIT_SUCCESS;
}
