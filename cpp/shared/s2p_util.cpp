//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "controllers/controller_factory.h"
#include "s2p_version.h"
#include "s2p_util.h"

using namespace std;
using namespace filesystem;

string s2p_util::GetVersionString()
{
    return fmt::format("{0}.{1}{2}{3}", s2p_major_version, s2p_minor_version,
        s2p_revision <= 0 ? "" : "." + to_string(s2p_revision), s2p_suffix);
}

vector<string> s2p_util::Split(const string &s, char separator, int limit)
{
    assert(limit >= 0);

    string component;
    vector<string> result;
    stringstream str(s);

    while (--limit > 0 && getline(str, component, separator)) {
        result.push_back(component);
    }

    if (!str.eof()) {
        getline(str, component);
        result.push_back(component);
    }

    return result;
}

string s2p_util::GetLocale()
{
    const char *locale = setlocale(LC_MESSAGES, "");
    if (locale == nullptr || !strcmp(locale, "C")) {
        locale = "en";
    }

    return locale;
}

bool s2p_util::GetAsUnsignedInt(const string &value, int &result)
{
    if (value.find_first_not_of("0123456789") != string::npos) {
        return false;
    }

    try {
        auto v = stoul(value);
        result = (int)v;
    }
    catch (const invalid_argument&) {
        return false;
    }
    catch (const out_of_range&) {
        return false;
    }

    return true;
}

string s2p_util::ProcessId(int id_max, int lun_max, const string &id_spec, int &id, int &lun)
{
    id = -1;
    lun = -1;

    if (id_spec.empty()) {
        return "Missing device ID";
    }

    if (const auto &components = Split(id_spec, COMPONENT_SEPARATOR, 2); !components.empty()) {
        if (components.size() == 1) {
            if (!GetAsUnsignedInt(components[0], id) || id >= id_max) {
                id = -1;

                return "Invalid device ID (0-" + to_string(id_max - 1) + ")";
            }

            return "";
        }

        if (!GetAsUnsignedInt(components[0], id) || id >= id_max || !GetAsUnsignedInt(components[1], lun)
            || lun >= lun_max) {
            id = -1;
            lun = -1;

            return "Invalid LUN (0-" + to_string(lun_max - 1) + ")";
        }
    }

    return "";
}

string s2p_util::Banner(string_view app)
{
    stringstream s;

    s << "SCSI Target Emulator and SCSI Initiator Tools SCSI2Pi " << app << "\n"
        << "Version " << GetVersionString();
    if (!s2p_suffix.empty()) {
        s << " (" << __DATE__ << ' ' << __TIME__ << ")";
    }
    s << "\nCopyright (C) 2016-2020 GIMONS\n"
        << "Copyright (C) 2020-2023 Contributors to the PiSCSI project\n"
        << "Copyright (C) 2023-2024 Uwe Seimet\n";

    return s.str();
}

string s2p_util::GetExtensionLowerCase(string_view filename)
{
    string ext;
    ranges::transform(path(filename).extension().string(), back_inserter(ext), ::tolower);

    // Remove the leading dot
    return ext.empty() ? "" : ext.substr(1);
}

void s2p_util::LogErrno(const string &msg)
{
    spdlog::error(errno ? msg + ": " + string(strerror(errno)) : msg);
}
