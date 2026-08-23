//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_formatter.h"
#include <spdlog/spdlog.h>

string S2pFormatter::FormatBytes(span<const uint8_t> bytes, size_t count, bool hex_only) const
{
    if (!format_limit) {
        return "";
    }

    string str;

    const size_t limit = min( { static_cast<size_t>(format_limit), bytes.size(), count });

    size_t offset = 0;
    while (offset < limit) {
        string output_offset;
        string output_hex;
        string output_ascii;

        if (!hex_only && !(offset % 16)) {
            output_offset = fmt::format("{:08x}  ", offset);
        }

        size_t index = 0;
        while (index < 16 && offset < limit) {
            if (index) {
                output_hex += ":";
            }
            output_hex += fmt::format("{:02x}", bytes[offset]);

            output_ascii += isprint(bytes[offset]) ? string(1, static_cast<char>(bytes[offset])) : ".";

            ++offset;
            ++index;
        }

        str += output_offset;
        str += fmt::format("{:47}", output_hex);
        if (!hex_only) {
            str += fmt::format("  '{}'", output_ascii);
        } else {
            const auto last_non_space = str.find_last_not_of(' ');
            if (last_non_space != string::npos) {
                str.erase(last_non_space + 1);
            }
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
