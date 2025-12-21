//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "protobuf_util.h"
#include <unistd.h>
#include "shared/s2p_exceptions.h"

// Serialize/Deserialize protobuf message: Length followed by the actual data.
// A little endian platform is assumed.
void protobuf_util::SerializeMessage(int fd, const google::protobuf::Message &message)
{
    vector<uint8_t> data(message.ByteSizeLong());
    message.SerializeToArray(data.data(), static_cast<int>(data.size()));

    // Write the size of the protobuf data as a header
    if (array<uint8_t, 4> header = { static_cast<uint8_t>(data.size()), static_cast<uint8_t>(data.size() >> 8),
        static_cast<uint8_t>(data.size() >> 16), static_cast<uint8_t>(data.size() >> 24) };
    WriteBytes(fd, header) != header.size()) {
        throw IoException("Can't write message size: " + string(strerror(errno)));
    }

    // Write the actual protobuf data
    if (WriteBytes(fd, data) != data.size()) {
        throw IoException("Can't write message data: " + string(strerror(errno)));
    }
}

void protobuf_util::DeserializeMessage(int fd, google::protobuf::Message &message)
{
    // Read the header with the size of the protobuf data
    array<byte, 4> header;
    if (ReadBytes(fd, header) != header.size()) {
        throw IoException("Can't read message size: " + string(strerror(errno)));
    }

    const int size = (static_cast<int>(header[3]) << 24) + (static_cast<int>(header[2]) << 16)
        + (static_cast<int>(header[1]) << 8) + static_cast<int>(header[0]);
    if (size < 0) {
        throw IoException("Invalid message size: " + string(strerror(errno)));
    }

    // Read the binary protobuf data
    vector<byte> data_buf(size);
    if (ReadBytes(fd, data_buf) != data_buf.size()) {
        throw IoException("Invalid message data: " + string(strerror(errno)));
    }

    message.ParseFromArray(data_buf.data(), size);
}

size_t protobuf_util::ReadBytes(int fd, span<byte> buf)
{
    size_t offset = 0;
    while (offset < buf.size()) {
        const auto len = read(fd, &buf.data()[offset], buf.size() - offset);
        if (len <= 0) {
            return !len ? offset : -1;
        }

        offset += len;
    }

    return offset;
}

size_t protobuf_util::WriteBytes(int fd, span<uint8_t> buf)
{
    size_t offset = 0;
    while (offset < buf.size()) {
        const auto len = write(fd, &buf.data()[offset], buf.size() - offset);
        if (len == -1) {
            return -1;
        }

        offset += len;
    }

    return offset;
}
