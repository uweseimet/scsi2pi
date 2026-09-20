//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_util.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <clocale>
#include <csignal>
#include <fcntl.h>
#if __has_include(<sys/ioctl.h>)
#include <sys/ioctl.h>
#endif
#if __has_include(<linux/fs.h>)
#include <linux/fs.h>
#include <sys/stat.h>
#endif
#if __has_include(<pwd.h>)
#include <pwd.h>
#endif
#include <unistd.h>
#include "s2p_exceptions.h"
#include "s2p_version.h"

string s2p_util::GetVersionString()
{
    if (s2p_revision > 0) {
        return fmt::format("{}.{}.{}{}", s2p_major_version, s2p_minor_version, s2p_revision, s2p_suffix);
    }
    return fmt::format("{}.{}{}", s2p_major_version, s2p_minor_version, s2p_suffix);
}

vector<string> s2p_util::Split(string_view s, char separator, int limit)
{
    assert(limit >= 0);

    const size_t n = s.size();
    size_t pos = 0;

    vector<string> result;

    while (--limit > 0) {
        if (pos == n) {
            return result;
        }

        const size_t sep_pos = s.find(separator, pos);
        if (sep_pos == string_view::npos) {
            break;
        }

        result.emplace_back(s.substr(pos, sep_pos - pos));

        pos = sep_pos + 1;
    }

    result.emplace_back(s.substr(pos));

    return result;
}

string s2p_util::ToUpper(string_view s)
{
    string result(s);
    ranges::transform(result, result.begin(), [](unsigned char c) {return static_cast<char>(std::toupper(c));});
    return result;
}

string s2p_util::ToLower(string_view s)
{
    string result(s);
    ranges::transform(result, result.begin(), [](unsigned char c) {return static_cast<char>(std::tolower(c));});
    return result;
}

string s2p_util::GetLocale()
{
#ifdef LC_MESSAGES
    const char *locale = setlocale(LC_MESSAGES, "");
#else
    const char *locale = setlocale(LC_ALL, "");
#endif
    if (locale == nullptr || !strcmp(locale, "C") || !strcmp(locale, "POSIX")) {
        locale = "en";
    }

    return locale;
}

string s2p_util::GetLine(const string &prompt, istream &in)
{
    string input;
    string line;

    const bool interactive = &in == &cin && isatty(STDIN_FILENO);

    while (true) {
        if (!line.ends_with('\\') && interactive) {
            cout << prompt << ">";
        }

        getline(in, line);

        if (const auto comment = line.find('#'); comment != string::npos) {
            line.resize(comment);
        }

        line = Trim(line);

        if (in.fail() || line == "exit" || line == "quit") {
            if (line.empty() && isatty(STDIN_FILENO)) {
                cout << "\n";
            }
            return "";
        }

        if (!line.empty() && !line.ends_with('\\')) {
            return input + line;
        }

        input += line.substr(0, line.size() - 1);
    }
}

int s2p_util::ParseAsUnsignedInt(const string &value)
{
    const string_view trimmed = Trim(value);
    if (trimmed.empty()) {
        return -1;
    }

    unsigned long result;
    const auto [ptr, ec] = from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);

    if (ec != errc() || ptr != trimmed.data() + trimmed.size()
        || result > static_cast<unsigned long>(numeric_limits<int>::max())) {
        return -1;
    }

    return static_cast<int>(result);
}

string s2p_util::ParseIdAndLun(const string &id_spec, int &id, int &lun)
{
    id = -1;
    lun = -1;

    if (id_spec.empty()) {
        return "Missing device ID";
    }

    if (const auto &components = Split(id_spec, COMPONENT_SEPARATOR, 2); !components.empty()) {
        id = ParseAsUnsignedInt(components[0]);
        if (id < 0 || id > 7) {
            id = -1;
            return "Invalid device ID: '" + components[0] + "' (0-7)";
        }

        if (components.size() > 1) {
            lun = ParseAsUnsignedInt(components[1]);
            if (lun < 0 || lun >= 32) {
                id = -1;
                lun = -1;
                return "Invalid LUN (0-31)";
            }
        }
    }

    return "";
}

string s2p_util::Banner(string_view app)
{
    return fmt::format("SCSI/SASI Device Emulator and SCSI Tools SCSI2Pi {}\n"
            "Version {}\n"
            "Copyright (C) 2016-2020 GIMONS\n"
            "Copyright (C) 2020-2023 Contributors to the PiSCSI project\n"
            "Copyright (C) 2021-2026 Uwe Seimet\n",
        app, GetVersionString());
}

vector<byte> s2p_util::HexToBytes(string_view hex)
{
    vector<byte> bytes;

    size_t pos = 0;
    while (pos < hex.size()) {
        const size_t nl = hex.find('\n', pos);
        const string_view line = nl == string_view::npos ? hex.substr(pos) : hex.substr(pos, nl - pos);
        pos = nl == string_view::npos ? hex.size() : nl + 1;

        if (line.starts_with(":") || line.ends_with(":")) {
            throw out_of_range("");
        }

        size_t i = 0;
        while (i < line.length()) {
            if (line[i] == ':' && i + 2 < line.length()) {
                ++i;
            }

            if (i + 1 >= line.length()) {
                throw out_of_range("");
            }

            const int b1 = HexToDec(line[i]);
            const int b2 = HexToDec(line[i + 1]);
            if (b1 == -1 || b2 == -1) {
                throw out_of_range("");
            }

            bytes.push_back(static_cast<byte>((b1 << 4) + b2));

            i += 2;
        }
    }

    return bytes;
}

string_view s2p_util::Trim(string_view s)
{
    if (const auto first = s.find_first_not_of(" \r"); first != string::npos) {
        const auto last = s.find_last_not_of(" \r");
        return s.substr(first, last - first + 1);
    }

    return "";
}

void s2p_util::SetTerminationHandler([[maybe_unused]] SignalHandlerPtr handler) // NOSONAR sigaction() requires a raw pointer
{
#ifdef SIGPIPE
    struct sigaction termination_handler = { };
    termination_handler.sa_handler = handler;

    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);
#endif
}
