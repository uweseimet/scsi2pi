//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2023-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "bus_factory.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include "shared/memory_util.h"
#include "in_process_bus.h"

using namespace spdlog;
using namespace memory_util;

BusFactory::BusFactory()
{
    // This mapping only contains the commands supported by s2p
    AddCommand(scsi_command::test_unit_ready, 6, "TEST UNIT READY", { 0, 0, false });
    AddCommand(scsi_command::rezero, 6, "REZERO/REWIND", { 0, 0, false });
    AddCommand(scsi_command::read_block_limits, 6, "READ BLOCK LIMITS", { -6, 0, false });
    AddCommand(scsi_command::request_sense, 6, "REQUEST SENSE", { 4, 1, false });
    AddCommand(scsi_command::format_unit, 6, "FORMAT UNIT/FORMAT MEDIUM", { 0, 0, false });
    AddCommand(scsi_command::reassign_blocks, 6, "REASSIGN BLOCKS", { 0, 0, false });
    AddCommand(scsi_command::read_6, 6, "READ(6)/GET MESSAGE(6)", { 4, 1, true });
    AddCommand(scsi_command::retrieve_stats, 6, "RETRIEVE STATS", { 4, 1, false });
    AddCommand(scsi_command::write_6, 6, "WRITE(6)/SEND MESSAGE(6)/PRINT", { 4, 1, true });
    AddCommand(scsi_command::seek_6, 6, "SEEK(6)", { 0, 0, false });
    AddCommand(scsi_command::set_iface_mode, 6, "SET INTERFACE MODE", { 0, 0, false });
    AddCommand(scsi_command::set_mcast_addr, 6, "SET MULTICAST ADDRESS", { 0, 0, false });
    AddCommand(scsi_command::enable_interface, 6, "ENABLE INTERFACE", { 0, 0, false });
    AddCommand(scsi_command::synchronize_buffer, 6, "SYNCHRONIZE BUFFER/WRITE_FILEMARKS(6)", { 0, 0, false });
    AddCommand(scsi_command::space_6, 6, "SPACE(6)", { 0, 0, false });
    AddCommand(scsi_command::inquiry, 6, "INQUIRY", { 4, 1, false });
    AddCommand(scsi_command::mode_select_6, 6, "MODE SELECT(6)", { 4, 1, false });
    AddCommand(scsi_command::reserve_6, 6, "RESERVE(6)", { 0, 0, false });
    AddCommand(scsi_command::release_6, 6, "RELEASE(6)", { 0, 0, false });
    AddCommand(scsi_command::erase_6, 6, "ERASE(6)", { 0, 0, false });
    AddCommand(scsi_command::mode_sense_6, 6, "MODE SENSE(6)", { 4, 1, false });
    AddCommand(scsi_command::start_stop, 6, "START STOP UNIT/STOP PRINT", { 0, 0, false });
    AddCommand(scsi_command::send_diagnostic, 6, "SEND DIAGNOSTIC", { 3, 2, false });
    AddCommand(scsi_command::prevent_allow_medium_removal, 6, "PREVENT ALLOW MEDIUM REMOVAL", { 0, 0, false });
    AddCommand(scsi_command::read_capacity_10, 10, "READ CAPACITY(10)", { -8, 0, false });
    AddCommand(scsi_command::read_10, 10, "READ(10)", { 7, 2, true });
    AddCommand(scsi_command::write_10, 10, "WRITE(10)", { 7, 2, true });
    AddCommand(scsi_command::seek_10, 10, "SEEK(10)/LOCATE(10)", { 0, 0, false });
    AddCommand(scsi_command::verify_10, 10, "VERIFY(10)", { 7, 2, true });
    AddCommand(scsi_command::synchronize_cache_10, 10, "SYNCHRONIZE CACHE(10)", { 0, 0, false });
    AddCommand(scsi_command::read_defect_data_10, 10, "READ DEFECT DATA(10)", { 7, 2, false });
    AddCommand(scsi_command::read_long_10, 10, "READ LONG(10)", { 7, 2, false });
    AddCommand(scsi_command::write_long_10, 10, "WRITE LONG(10)", { 7, 2, false });
    AddCommand(scsi_command::read_toc, 10, "READ TOC", { 7, 2, false });
    AddCommand(scsi_command::mode_select_10, 10, "MODE SELECT(10)", { 7, 2, false });
    AddCommand(scsi_command::mode_sense_10, 10, "MODE SENSE(10)", { 7, 2, false });
    AddCommand(scsi_command::read_16, 16, "READ(16)", { 10, 4, true });
    AddCommand(scsi_command::write_16, 16, "WRITE(16)", { 10, 4, true });
    AddCommand(scsi_command::verify_16, 16, "VERIFY(16)", { 10, 4, true });
    AddCommand(scsi_command::read_position, 10, "READ POSITION", { 7, 2, false });
    AddCommand(scsi_command::synchronize_cache_16, 16, "SYNCHRONIZE CACHE(16)", { 0, 0, false });
    AddCommand(scsi_command::locate_16, 16, "LOCATE(16)", { 0, 0, false });
    AddCommand(scsi_command::read_capacity_16_read_long_16, 16, "READ CAPACITY(16)/READ LONG(16)", { 12, 2, false });
    AddCommand(scsi_command::write_long_16, 16, "WRITE LONG(16)", { 12, 2, false });
    AddCommand(scsi_command::report_luns, 12, "REPORT LUNS", { 6, 4, false });
    AddCommand(scsi_command::execute_operation, 10, "EXECUTE OPERATION (SCSI2Pi-specific)", { 7, 2, false });
    AddCommand(scsi_command::receive_operation_results, 10, "RECEIVE OPERATION RESULTS (SCSI2Pi-specific)", { 7, 2,
        false });
}

void BusFactory::AddCommand(scsi_command opcode, int byte_count, const char *name, const AllocationLengthDesc &desc)
{
    command_byte_counts[static_cast<int>(opcode)] = byte_count;
    command_names[static_cast<int>(opcode)] = name;
    assert(desc.offset <= 12);
    assert(!desc.size || desc.size == 1 || desc.size == 2 || desc.size == 3 || desc.size == 4 || desc.size == 8);
    allocation_length_descs[static_cast<int>(opcode)] = desc;
}

int BusFactory::GetAllocationLength(span<const int> cdb)
{
    const AllocationLengthDesc &desc = allocation_length_descs[cdb[0]];

    // For commands without allocation length field the length is coded as a negative offset
    if (desc.offset < 0) {
        return -desc.offset;
    }

    int allocation_length = 0;
    switch (desc.size) {
    case 0:
        break;

    case 1:
        allocation_length = cdb[desc.offset];
        break;

    case 2:
        allocation_length = GetInt16(cdb, desc.offset);
        break;

    case 3:
        allocation_length = GetInt24(cdb, desc.offset);
        break;

    case 4:
        allocation_length = GetInt32(cdb, desc.offset);
        break;

    case 8:
        allocation_length = GetInt64(cdb, desc.offset);
        break;

    default:
        assert(false);
        break;
    }

    // TODO Try to support other block sizes than 512 bytes, e.g. by running READ CAPACITY on startup
    return desc.block ? 512 * allocation_length : allocation_length;
}

unique_ptr<Bus> BusFactory::CreateBus(bool target, bool in_process, bool log_signals)
{
    unique_ptr<Bus> bus;

    if (in_process) {
        bus = make_unique<DelegatingInProcessBus>(InProcessBus::Instance(), log_signals);
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

