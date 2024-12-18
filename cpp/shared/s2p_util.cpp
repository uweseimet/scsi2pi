//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_util.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <pwd.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include "s2p_exceptions.h"
#include "s2p_version.h"

using namespace filesystem;
using namespace spdlog;

string s2p_util::GetVersionString()
{
    const string &revision = s2p_revision <= 0 ? "" : "." + to_string(s2p_revision);
    return fmt::format("{0}.{1}{2}{3}", s2p_major_version, s2p_minor_version, revision, s2p_suffix);
}

string s2p_util::GetHomeDir()
{
    const auto [uid, gid] = GetUidAndGid();

    passwd pwd = { };
    passwd *p_pwd;
    array<char, 256> pwbuf;

    if (uid && !getpwuid_r(uid, &pwd, pwbuf.data(), pwbuf.size(), &p_pwd)) {
        return pwd.pw_dir;
    }
    else {
        return "/home/pi";
    }
}

pair<int, int> s2p_util::GetUidAndGid()
{
    int uid = getuid();
    if (const char *sudo_user = getenv("SUDO_UID"); sudo_user) {
        uid = stoi(sudo_user);
    }

    passwd pwd = { };
    passwd *p_pwd;
    array<char, 256> pwbuf;

    int gid = -1;
    if (!getpwuid_r(uid, &pwd, pwbuf.data(), pwbuf.size(), &p_pwd)) {
        gid = pwd.pw_gid;
    }

    return {uid, gid};
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

string s2p_util::ToUpper(const string &s)
{
    string result;
    ranges::transform(s, back_inserter(result), ::toupper);
    return result;
}

string s2p_util::ToLower(const string &s)
{
    string result;
    ranges::transform(s, back_inserter(result), ::tolower);
    return result;
}

string s2p_util::GetExtensionLowerCase(string_view filename)
{
    const string &ext = ToLower(filesystem::path(filename).extension().string());

    // Remove the leading dot
    return ext.empty() ? "" : ext.substr(1);
}

string s2p_util::GetLocale()
{
    const char *locale = setlocale(LC_MESSAGES, "");
    if (locale == nullptr || !strcmp(locale, "C") || !strcmp(locale, "POSIX")) {
        locale = "en";
    }

    return locale;
}

string s2p_util::GetLine(const string &prompt)
{
    string input;
    string line;
    while (true) {
        if (!line.ends_with('\\') && isatty(STDIN_FILENO)) {
            cout << prompt << ">";
        }

        getline(cin, line);
        line = Trim(line);

        if (cin.fail() || line == "exit" || line == "quit") {
            if (line.empty() && isatty(STDIN_FILENO)) {
                cout << "\n";
            }
            return "";
        }

        if (line.starts_with('#')) {
            continue;
        }

        if (!line.empty() && !line.ends_with('\\')) {
            return input + line;
        }

        input += line.substr(0, line.size() - 1);
    }
}

bool s2p_util::GetAsUnsignedInt(const string &value, int &result)
{
    if (value.find_first_not_of(" 0123456789 ") != string::npos) {
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

string s2p_util::ProcessId(const string &id_spec, int &id, int &lun)
{
    id = -1;
    lun = -1;

    if (id_spec.empty()) {
        return "Missing device ID";
    }

    if (const auto &components = Split(id_spec, COMPONENT_SEPARATOR, 2); !components.empty()) {
        if (components.size() == 1) {
            if (!GetAsUnsignedInt(components[0], id) || id > 7) {
                id = -1;
                return "Invalid device ID: '" + components[0] + "' (0-7)";
            }

            return "";
        }

        if (!GetAsUnsignedInt(components[0], id) || id > 7 || !GetAsUnsignedInt(components[1], lun) || lun >= 32) {
            id = -1;
            lun = -1;
            return "Invalid LUN (0-31)";
        }
    }

    return "";
}

string s2p_util::Banner(string_view app)
{
    stringstream s;

    s << "SCSI Device Emulator and SCSI Tools SCSI2Pi " << app << "\n"
        << "Version " << GetVersionString() << "\n"
        << "Copyright (C) 2016-2020 GIMONS\n"
        << "Copyright (C) 2020-2023 Contributors to the PiSCSI project\n"
        << "Copyright (C) 2021-2024 Uwe Seimet\n";

    return s.str();
}

string s2p_util::GetScsiLevel(int scsi_level)
{
    switch (scsi_level) {
    case 0:
        return "???";
        break;

    case 1:
        return "SCSI-1-CCS";
        break;

    case 2:
        return "SCSI-2";
        break;

    case 3:
        return "SCSI-3 (SPC)";
        break;

    default:
        return "SPC-" + to_string(scsi_level - 2);
        break;
    }
}

string s2p_util::FormatSenseData(sense_key sense_key, asc asc, int ascq)
{
    string s_asc;
    if (const auto &it_asc = ASC_MAPPING.find(asc); it_asc != ASC_MAPPING.end()) {
        s_asc = fmt::format("{0} (ASC ${1:02x}), ASCQ ${2:02x}", it_asc->second, static_cast<int>(asc), ascq);
    }
    else {
        s_asc = fmt::format("ASC ${0:02x}, ASCQ ${1:02x}", static_cast<int>(asc), ascq);
    }

    return fmt::format("{0} (Sense Key ${1:02x}), {2}", SENSE_KEYS[static_cast<int>(sense_key)],
        static_cast<int>(sense_key), s_asc);
}

vector<byte> s2p_util::HexToBytes(const string &hex)
{
    vector<byte> bytes;

    stringstream ss(hex);
    string line;
    while (getline(ss, line)) {
        if (line.starts_with(":") || line.ends_with(":")) {
            throw out_of_range("");
        }

        const string &line_lower = ToLower(line);

        size_t i = 0;
        while (i < line_lower.length()) {
            if (line_lower[i] == ':' && i + 2 < line_lower.length()) {
                i++;
            }

            bytes.push_back(static_cast<byte>((HexToDec(line_lower[i]) << 4) + HexToDec(line_lower[i + 1])));

            i += 2;
        }
    }

    return bytes;
}

string s2p_util::FormatBytes(span<const uint8_t> bytes, int count, bool hex_only)
{
    string str;

    int offset = 0;
    while (offset < count) {
        string output_offset;
        string output_hex;
        string output_ascii;

        if (!hex_only && !(offset % 16)) {
            output_offset += fmt::format("{:08x}  ", offset);
        }

        int index = -1;
        while (++index < 16 && offset < count) {
            if (index) {
                output_hex += ":";
            }
            output_hex += fmt::format("{:02x}", bytes[offset]);

            output_ascii += isprint(bytes[offset]) ? string(1, static_cast<char>(bytes[offset])) : ".";

            ++offset;
        }

        str += output_offset;
        str += fmt::format("{:47}", output_hex);
        str += hex_only ? "" : fmt::format("  '{}'", output_ascii);

        if (hex_only) {
            str.erase(str.find_last_not_of(' ') + 1);
        }

        if (offset < count) {
            str += "\n";
        }
    }

    return str;
}

int s2p_util::HexToDec(char c)
{
    if (c >= '0' && c <= '9') {
        return c -'0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    throw out_of_range("");
}

string s2p_util::Trim(const string &s)
{
    const size_t first = s.find_first_not_of(' ');
    if (first == string::npos) {
        return s;
    }
    const size_t last = s.find_last_not_of(' ');
    return s.substr(first, (last - first + 1));
}
