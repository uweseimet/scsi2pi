//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <filesystem>
#include <fstream>
#include <sstream>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include "s2pproto_executor.h"

using namespace filesystem;
using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace s2p_interface;

string S2pProtoExecutor::Execute(const string &filename, protobuf_format input_format, PbResult &result)
{
    int length = 0;

    switch (input_format) {
    case protobuf_format::binary: {
        ifstream in(filename, ios::binary);
        if (in.fail()) {
            return "Can't open input file '" + filename + "': " + strerror(errno);
        }

        length = file_size(filename);
        vector<char> data(length);
        in.read(data.data(), length);
        memcpy(buffer.data(), data.data(), length);
        break;
    }

    case protobuf_format::json:
        case protobuf_format::text: {
        ifstream in(filename);
        if (in.fail()) {
            return "Can't open input file '" + filename + "': " + strerror(errno);
        }

        stringstream buf;
        buf << in.rdbuf();
        const string &data = buf.str();
        length = data.size();
        memcpy(buffer.data(), data.data(), length);
        break;
    }

    default:
        assert(false);
        break;
    }

    array<uint8_t, 10> cdb = { };
    cdb[1] = static_cast<uint8_t>(input_format);
    cdb[7] = static_cast<uint8_t>(length >> 8);
    cdb[8] = static_cast<uint8_t>(length);

    if (initiator_executor->Execute(scsi_command::cmd_execute_operation, cdb, buffer, length)) {
        return "Can't execute operation";
    }

    cdb[7] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[8] = static_cast<uint8_t>(buffer.size());

    if (initiator_executor->Execute(scsi_command::cmd_receive_operation_results, cdb, buffer, buffer.size())) {
        return "Can't read operation result";
    }

    switch (input_format) {
    case protobuf_format::binary: {
        if (!result.ParseFromArray(buffer.data(), initiator_executor->GetByteCount())) {
            return "Can't parse binary protobuf data";
        }
        break;
    }

    case protobuf_format::json: {
        const string json((const char*)buffer.data(), initiator_executor->GetByteCount());
        if (!JsonStringToMessage(json, &result).ok()) {
            return "Can't parse JSON protobuf data";
        }
        break;
    }

    case protobuf_format::text: {
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
