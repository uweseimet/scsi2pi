//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "protobuf_util.h"
#include <unistd.h>
#include <google/protobuf/message_lite.h>
#include "shared/s2p_exceptions.h"

// Serialize/Deserialize protobuf message: Length followed by the actual data.
void protobuf_util::SerializeMessage(int fd, const google::protobuf::MessageLite &message)
{
    vector<uint8_t> data(message.ByteSizeLong());
    if (!message.SerializeToArray(data.data(), static_cast<int>(data.size()))) {
        throw IoException("Can't serialize message");
    }

    // Write the size of the protobuf data as a header
    const array<uint8_t, 4> header = { static_cast<uint8_t>(data.size()), static_cast<uint8_t>(data.size() >> 8),
        static_cast<uint8_t>(data.size() >> 16), static_cast<uint8_t>(data.size() >> 24) };
    WriteBytes(fd, header);

    // Write the payload
    WriteBytes(fd, data);
}

void protobuf_util::DeserializeMessage(int fd, google::protobuf::MessageLite &message)
{
    // Read the header with the size of the protobuf data
    array<byte, 4> header;
    if (!ReadBytes(fd, header)) {
        throw IoException("Can't read message size");
    }

    const uint32_t raw_size = (to_integer<uint32_t>(header[3]) << 24) | (to_integer<uint32_t>(header[2]) << 16)
        | (to_integer<uint32_t>(header[1]) << 8) | to_integer<uint32_t>(header[0]);
    const auto size = static_cast<int32_t>(raw_size);
    if (size < 0) {
        throw IoException("Invalid message size");
    }

    // Read the payload
    vector<byte> data_buf(size);
    if (!ReadBytes(fd, data_buf)) {
        throw IoException("Invalid message data");
    }

    if (!message.ParseFromArray(data_buf.data(), size)) {
        throw IoException("Can't parse protobuf message");
    }
}

bool protobuf_util::ReadBytes(int fd, span<byte> buf)
{
    size_t offset = 0;
    while (offset < buf.size()) {
        const auto len = read(fd, &buf.data()[offset], buf.size() - offset);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw IoException("Can't read {} message bytes: {}", buf.size(),
                system_error(errno, generic_category()).what());
        }

        if (!len) {
            break;
        }

        offset += len;
    }

    return offset == buf.size();
}

void protobuf_util::WriteBytes(int fd, span<const uint8_t> buf)
{
    size_t offset = 0;
    while (offset < buf.size()) {
        const auto len = write(fd, &buf.data()[offset], buf.size() - offset);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw IoException("Can't write {} message bytes: {}", buf.size(),
                system_error(errno, generic_category()).what());
        }

        offset += len;
    }
}
