//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include "s2p_formatter.h"
#include "s2p_util.h"

using namespace spdlog;
using namespace s2p_util;

string S2pFormatter::FormatBytes(span<const uint8_t> bytes, size_t count, bool hex_only) const
{
    if (!format_limit) {
        return "";
    }

    string str;

    size_t limit = format_limit;
    if (limit > count) {
        limit = count;
    }

    size_t offset = 0;
    while (offset < limit) {
        string output_offset;
        string output_hex;
        string output_ascii;

        if (!hex_only && !(offset % 16)) {
            output_offset += fmt::format("{:08x}  ", offset);
        }

        size_t index = -1;
        while (++index < 16 && offset < limit) {
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

        if (offset < limit) {
            str += "\n";
        }
    }

    if (count > limit) {
        str += fmt::format("\n... ({} more)", count - limit);
    }

    return str;
}
