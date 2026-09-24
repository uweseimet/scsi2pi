//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_util.h"
#include <cassert>
#include <cstring>
#include <ctime> // NOSONAR Using nanosleep cannot be avoided
#include <utility>
#include <spdlog/fmt/fmt.h>
#include "memory_util.h"

using namespace memory_util;

tuple<string, string, string> scsi_util::GetInquiryProductData(span<const uint8_t> data)
{
    assert(data.size() >= 36);

    array<char, 9> vendor = { };
    memcpy(vendor.data(), &data[8], 8);
    array<char, 17> product = { };
    memcpy(product.data(), &data[16], 16);
    array<char, 5> revision = { };
    memcpy(revision.data(), &data[32], 4);

    return {vendor.data(),product.data(), revision.data()};
}

string scsi_util::GetScsiLevel(int scsi_level)
{
    switch (scsi_level) {
    case 0:
        return "-";

    case 1:
        return "SCSI-1-CCS";

    case 2:
        return "SCSI-2";

    case 3:
        return "SCSI-3 (SPC)";

    default:
        return fmt::format("SPC-{}", scsi_level - 2);
    }
}

string scsi_util::GetStatusString(int status_code)
{
    if (const auto &it = STATUS_MAPPING.find(static_cast<StatusCode>(status_code)); it != STATUS_MAPPING.end()) {
        return fmt::format("Device reported {} (status code ${:02x})", it->second, status_code);
    }
    else if (status_code != 0xff) {
        return fmt::format("Device reported an unknown status (status code ${:02x})", status_code);
    }

    return "Device did not respond";
}

string scsi_util::FormatSenseData(span<const byte> sense_data)
{
    assert(sense_data.size() >= 14);

    const byte flags = sense_data[2];

    const string &s = FormatSenseData(static_cast<SenseKey>(flags & byte { 0x0f }), static_cast<Asc>(sense_data[12]),
        static_cast<Ascq>(sense_data[13]));

    if ((sense_data[0] & byte { 0x80 }) == byte { 0 }) {
        return s;
    }

    return s
        + fmt::format(", EOM: {}, ILI: {}, INFORMATION: {}", (flags & byte { 0x40 }) != byte { 0 } ? "1" : "0",
            (flags & byte { 0x20 }) != byte { 0 } ? "1" : "0",
        static_cast<int>(GetInt32(sense_data, 3)));
}

string scsi_util::FormatSenseData(SenseKey sense_key, Asc asc, Ascq ascq)
{
    assert(to_underlying(sense_key) < 16);

    string s_asc;
    if (const auto &it_asc = ASC_MAPPING.find(asc); it_asc != ASC_MAPPING.end()) {
        s_asc = fmt::format("{} (ASC ${:02x}), ASCQ ${:02x}", it_asc->second, to_underlying(asc), to_underlying(ascq));
    }
    else {
        s_asc = fmt::format("ASC ${:02x}, ASCQ ${:02x}", to_underlying(asc), to_underlying(ascq));
    }

    return fmt::format("{} (Sense Key ${:02x}), {}", SENSE_KEYS[to_underlying(sense_key)], to_underlying(sense_key),
        s_asc);
}

void scsi_util::Sleep(const timespec &ns)
{
    nanosleep(&ns, nullptr);
}
