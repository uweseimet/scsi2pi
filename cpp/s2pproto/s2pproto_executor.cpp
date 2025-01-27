//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pproto_executor.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include "generated/s2p_interface.pb.h"

using namespace filesystem;
using namespace google::protobuf;
using namespace google::protobuf::util;

string S2pProtoExecutor::Execute(const string &filename, ProtobufFormat input_format, PbResult &result)
{
    ifstream in(filename, input_format == ProtobufFormat::BINARY ? ios::binary : ios::in);
    if (!in) {
        return "Can't open input file '" + filename + "': " + strerror(errno);
    }

    int length = 0;
    switch (input_format) {
    case ProtobufFormat::BINARY:
        length = file_size(filename);
        buffer.resize(length);
        in.read((char*)buffer.data(), length);
        break;

    case ProtobufFormat::JSON:
    case ProtobufFormat::TEXT: {
        stringstream buf;
        buf << in.rdbuf();
        const string &data = buf.str();
        length = data.size();
        buffer.resize(length);
        memcpy(buffer.data(), data.data(), length);
        break;
    }

    default:
        assert(false);
        break;
    }

    if (buffer.size() > BUFFER_SIZE) {
        return "Buffer overflow";
    }

    array<uint8_t, 10> cdb = { };
    cdb[0] = static_cast<uint8_t>(ScsiCommand::EXECUTE_OPERATION);
    cdb[1] = static_cast<uint8_t>(input_format);
    cdb[7] = static_cast<uint8_t>(length >> 8);
    cdb[8] = static_cast<uint8_t>(length);

    if (initiator_executor->Execute(cdb, buffer, length, 3, true)) {
        return "Can't execute operation";
    }

    cdb[0] = static_cast<uint8_t>(ScsiCommand::RECEIVE_OPERATION_RESULTS);
    cdb[7] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[8] = static_cast<uint8_t>(buffer.size());

    if (initiator_executor->Execute(cdb, buffer, buffer.size(), 3, true)) {
        return "Can't read operation result";
    }

    switch (input_format) {
    case ProtobufFormat::BINARY: {
        if (!result.ParseFromArray(buffer.data(), initiator_executor->GetByteCount())) {
            return "Can't parse binary protobuf data";
        }
        break;
    }

    case ProtobufFormat::JSON: {
        const string json((const char*)buffer.data(), initiator_executor->GetByteCount());
        if (!JsonStringToMessage(json, &result).ok()) {
            return "Can't parse JSON protobuf data";
        }
        break;
    }

    case ProtobufFormat::TEXT: {
        const string text((const char*)buffer.data(), initiator_executor->GetByteCount());
        if (!TextFormat::ParseFromString(text, &result)) {
            return "Can't parse text format protobuf data";
        }
        break;
    }

    default:
        assert(false);
        break;
    }

    return "";
}
