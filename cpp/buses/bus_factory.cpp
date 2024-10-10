//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include "in_process_bus.h"

using namespace spdlog;

BusFactory::BusFactory()
{
    // This mapping only contains the commands supported by s2p
    AddCommand(scsi_command::cmd_test_unit_ready, 6, "TEST UNIT READY");
    AddCommand(scsi_command::cmd_rezero, 6, "REZERO/REWIND");
    AddCommand(scsi_command::cmd_read_block_limits, 6, "READ BLOCK LIMITS");
    AddCommand(scsi_command::cmd_request_sense, 6, "REQUEST SENSE");
    AddCommand(scsi_command::cmd_format_unit, 6, "FORMAT UNIT/FORMAT MEDIUM");
    AddCommand(scsi_command::cmd_reassign_blocks, 6, "REASSIGN BLOCKS");
    AddCommand(scsi_command::cmd_read6, 6, "READ(6)/GET MESSAGE(6)");
    AddCommand(scsi_command::cmd_retrieve_stats, 6, "RETRIEVE STATS");
    AddCommand(scsi_command::cmd_write6, 6, "WRITE(6)/SEND MESSAGE(6)/PRINT");
    AddCommand(scsi_command::cmd_seek6, 6, "SEEK(6)");
    AddCommand(scsi_command::cmd_set_iface_mode, 6, "SET INTERFACE MODE");
    AddCommand(scsi_command::cmd_set_mcast_addr, 6, "SET MULTICAST ADDRESS");
    AddCommand(scsi_command::cmd_enable_interface, 6, "ENABLE INTERFACE");
    AddCommand(scsi_command::cmd_synchronize_buffer, 6, "SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)");
    AddCommand(scsi_command::cmd_space6, 6, "SPACE(6)");
    AddCommand(scsi_command::cmd_inquiry, 6, "INQUIRY");
    AddCommand(scsi_command::cmd_mode_select6, 6, "MODE SELECT(6)");
    AddCommand(scsi_command::cmd_reserve6, 6, "RESERVE(6)");
    AddCommand(scsi_command::cmd_release6, 6, "RELEASE(6)");
    AddCommand(scsi_command::cmd_erase6, 6, "ERASE(6)");
    AddCommand(scsi_command::cmd_mode_sense6, 6, "MODE SENSE(6)");
    AddCommand(scsi_command::cmd_start_stop, 6, "START STOP UNIT/STOP PRINT");
    AddCommand(scsi_command::cmd_send_diagnostic, 6, "SEND DIAGNOSTIC");
    AddCommand(scsi_command::cmd_prevent_allow_medium_removal, 6, "PREVENT ALLOW MEDIUM REMOVAL");
    AddCommand(scsi_command::cmd_read_capacity10, 10, "READ CAPACITY(10)");
    AddCommand(scsi_command::cmd_read10, 10, "READ(10)");
    AddCommand(scsi_command::cmd_write10, 10, "WRITE(10)");
    AddCommand(scsi_command::cmd_seek10, 10, "SEEK(10)/LOCATE(10)");
    AddCommand(scsi_command::cmd_verify10, 10, "VERIFY(10)");
    AddCommand(scsi_command::cmd_synchronize_cache10, 10, "SYNCHRONIZE CACHE(10)");
    AddCommand(scsi_command::cmd_read_defect_data10, 10, "READ DEFECT DATA(10)");
    AddCommand(scsi_command::cmd_read_long10, 10, "READ LONG(10)");
    AddCommand(scsi_command::cmd_write_long10, 10, "WRITE LONG(10)");
    AddCommand(scsi_command::cmd_read_toc, 10, "READ TOC");
    AddCommand(scsi_command::cmd_mode_select10, 10, "MODE SELECT(10)");
    AddCommand(scsi_command::cmd_mode_sense10, 10, "MODE SENSE(10)");
    AddCommand(scsi_command::cmd_read16, 16, "READ(16)");
    AddCommand(scsi_command::cmd_write16, 16, "WRITE(16)");
    AddCommand(scsi_command::cmd_verify16, 16, "VERIFY(16)");
    AddCommand(scsi_command::cmd_read_position, 10, "READ POSITION");
    AddCommand(scsi_command::cmd_synchronize_cache16, 16, "SYNCHRONIZE CACHE(16)");
    AddCommand(scsi_command::cmd_locate16, 16, "LOCATE(16)");
    AddCommand(scsi_command::cmd_read_capacity16_read_long16, 16, "READ CAPACITY(16)/READ LONG(16)");
    AddCommand(scsi_command::cmd_write_long16, 16, "WRITE LONG(16)");
    AddCommand(scsi_command::cmd_report_luns, 12, "REPORT LUNS");
    AddCommand(scsi_command::cmd_execute_operation, 10, "EXECUTE OPERATION (SCSI2Pi-specific)");
    AddCommand(scsi_command::cmd_receive_operation_results, 10, "RECEIVE OPERATION RESULTS (SCSI2Pi-specific)");
}

void BusFactory::AddCommand(scsi_command opcode, int byte_count, const char *name)
{
    command_byte_counts[static_cast<int>(opcode)] = byte_count;
    command_names[static_cast<int>(opcode)] = name;
}

unique_ptr<Bus> BusFactory::CreateBus(bool target, bool in_process)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), true);
    }
    else if (const auto pi_type = CheckForPi(); pi_type != RpiBus::PiType::unknown) {
        bus = make_unique<RpiBus>(pi_type);
    }
    else {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), false);
    }

    if (bus->Init(target)) {
        bus->Reset();
        return bus;
    }

    return nullptr;
}

RpiBus::PiType BusFactory::CheckForPi()
{
    ifstream in("/proc/device-tree/model");
    if (in.fail()) {
        warn("This platform is not a Raspberry Pi, functionality is limited");
        return RpiBus::PiType::unknown;
    }

    stringstream s;
    s << in.rdbuf();
    const string &model = s.str();

    if (!model.starts_with("Raspberry Pi ") || model.size() < 13) {
        warn("This platform is not a Raspberry Pi, functionality is limited");
        return RpiBus::PiType::unknown;
    }

    const int type = model.find("Zero") != string::npos ? 1 : model.substr(13, 1)[0] - '0';
    if (type <= 0 || type > 4) {
        warn("Unsupported Raspberry Pi model '{}', functionality is limited", model);
        return RpiBus::PiType::unknown;
    }

    return static_cast<RpiBus::PiType>(type);
}

