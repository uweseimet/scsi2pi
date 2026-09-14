//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "user_util.h"
#include <array>
#include <filesystem>
#if __has_include(<pwd.h>)
#include <pwd.h>
#endif
#include <unistd.h>

using namespace filesystem;

namespace
{

tuple<int, int, string> GetPwData()
{
#if __has_include(<pwd.h>)
    const char *sudo_user = getenv("SUDO_UID");
    const int uid = sudo_user ? stoi(sudo_user) : user_util::GetEuid();

    if (array<char, 256> pwbuf; uid != -1) {
        passwd pwd = { };
        passwd *p_pwd = nullptr;
        if (!getpwuid_r(uid, &pwd, pwbuf.data(), pwbuf.size(), &p_pwd) && p_pwd != nullptr) {
            if (error_code error; exists(user_util::DEFAULT_APP_FOLDER, error)) {
                return {uid, pwd.pw_gid, user_util::DEFAULT_APP_FOLDER};
            }
            else {
                // For backward compatibility
                const string &dir = uid ? pwd.pw_dir : "/home/pi";
                return {uid, pwd.pw_gid, exists(dir, error) ? dir : user_util::DEFAULT_APP_FOLDER};
            }
        }
    }
#endif

    return {-1, -1 , user_util::DEFAULT_APP_FOLDER};
}

}

string user_util::GetAppDir()
{
    return get<2>(GetPwData());
}

int user_util::GetEuid()
{
#if __has_include(<pwd.h>)
    return geteuid();
#else
    return -1;
#endif
}

pair<int, int> user_util::GetUidAndGid()
{
    const auto& [uid, gid, _] = GetPwData(); // NOSONAR '_' will be supported in C++-26
    return {uid, gid};
}
