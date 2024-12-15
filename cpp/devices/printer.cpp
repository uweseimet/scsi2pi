//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// Implementation of a SCSI printer (see SCSI-2 specification for a command description)
//
//---------------------------------------------------------------------------

//
// How to print:
//
// 1. The client sends the data to be printed with one or several PRINT commands. The maximum
// transfer size per PRINT command should not exceed 4096 bytes, in order to be compatible with
// PiSCSI and to save memory.
// 2. The client triggers printing with SYNCHRONIZE BUFFER. Each SYNCHRONIZE BUFFER results in
// the print command for this printer (see below) to be called for the data not yet printed.
//
// It is recommended to reserve the printer device before printing and to release it afterwards.
// The command to be used for printing can be set with the "cmd" property when attaching the device.
// By default the data to be printed are sent to the printer unmodified, using "lp -oraw %f". This
// requires that the client uses a printer driver compatible with the respective printer, or that the
// printing service on the Pi is configured to do any necessary conversions, or that the print command
// applies any conversions on the file to be printed (%f) before passing it to the printing service.
// 'enscript' is an example for a conversion tool.
// By attaching different devices/LUNs multiple printers (i.e. different print commands) are possible.
//
// With STOP PRINT printing can be cancelled before SYNCHRONIZE BUFFER was sent.
//

#include "printer.h"
#include <filesystem>
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace memory_util;

Printer::Printer(int lun) : PrimaryDevice(SCLP, lun)
{
    PrimaryDevice::SetProductData( { "", "SCSI PRINTER", "" });
    SupportsParams(true);
    SetReady(true);
}

bool Printer::SetUp()
{
    if (GetParam(CMD).find("%f") == string::npos) {
        LogError("Missing filename specifier '%f'");
        return false;
    }

    AddCommand(scsi_command::print, [this]
        {
            Print();
        });
    AddCommand(scsi_command::synchronize_buffer, [this]
        {
            SynchronizeBuffer();
        });
    AddCommand(scsi_command::stop_print, [this]
        {
            StatusPhase();
        });

    error_code error;
    file_template = temp_directory_path(error); // NOSONAR Publicly writable directory is fine here
    file_template += PRINTER_FILE_PATTERN;

    return true;
}

void Printer::CleanUp()
{
    if (out.is_open()) {
        out.close();
    }

    if (!filename.empty()) {
        error_code error;
        filesystem::remove(path(filename), error);

        filename.clear();
    }
}

param_map Printer::GetDefaultParams() const
{
    return {
        {   CMD, "lp -oraw %f"}
    };
}

vector<uint8_t> Printer::InquiryInternal() const
{
    return HandleInquiry(device_type::printer, false);
}

void Printer::Print()
{
    const uint32_t length = GetCdbInt24(2);

    LogTrace(fmt::format("Expecting to receive {} byte(s) for printing", length));

    if (length > GetController()->GetBuffer().size()) {
        LogError(
            fmt::format("Transfer buffer overflow: Buffer size is {0} bytes, {1} byte(s) expected",
                GetController()->GetBuffer().size(), length));

        ++print_error_count;

        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    GetController()->SetTransferSize(length, length);

    DataOutPhase(length);
}

void Printer::SynchronizeBuffer()
{
    if (!out.is_open()) {
        LogWarn("Nothing to print");

        ++print_warning_count;

        throw scsi_exception(sense_key::aborted_command, asc::printer_nothing_to_print);
    }

    out.close();

    string cmd = GetParam(CMD);
    const size_t file_position = cmd.find("%f");
    // The format has been verified before
    assert(file_position != string::npos);
    cmd.replace(file_position, 2, filename);

    error_code error;
    LogTrace(fmt::format("Printing file '{0}' with {1} byte(s)", filename, file_size(path(filename), error)));

    LogDebug(fmt::format("Executing print command '{}'", cmd));

    if (system(cmd.c_str())) {
        LogError(fmt::format("Printing file '{}' failed, the Pi's printing system might not be configured", filename));

        ++print_error_count;

        CleanUp();

        throw scsi_exception(sense_key::aborted_command, asc::printer_printing_failed);
    }

    CleanUp();

    StatusPhase();
}

int Printer::WriteData(cdb_t cdb, data_out_t buf, int, int l)
{
    const auto command = static_cast<scsi_command>(cdb[0]);
    assert(command == scsi_command::print);
    if (command != scsi_command::print) {
        throw scsi_exception(sense_key::aborted_command);
    }

    const auto length = GetCdbInt24(2);

    byte_receive_count += length;

    if (!out.is_open()) {
        vector<char> f(file_template.begin(), file_template.end());
        f.push_back(0);

        // There is no C++ API that generates a file with a unique name
        const int fd = mkstemp(f.data());
        if (fd == -1) {
            LogError(fmt::format("Can't create printer output file for pattern '{0}': {1}", filename, strerror(errno)));
            ++print_error_count;
            throw scsi_exception(sense_key::aborted_command, asc::printer_write_failed);
        }
        close(fd);

        filename = f.data();

        out.open(filename, ios::binary);
        CheckForFileError();

        LogTrace("Created printer output file '" + filename + "'");
    }

    LogTrace(fmt::format("Appending {0} byte(s) to printer output file '{1}'", length, filename));

    out.write((const char*)buf.data(), length);
    CheckForFileError();

    return l;
}

void Printer::CheckForFileError()
{
    if (out.fail()) {
        out.clear();
        ++print_error_count;
        throw scsi_exception(sense_key::aborted_command, asc::printer_write_failed);
    }
}

vector<PbStatistics> Printer::GetStatistics() const
{
    vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

    PbStatistics s;
    s.set_id(GetId());
    s.set_unit(GetLun());

    s.set_category(PbStatisticsCategory::CATEGORY_INFO);

    s.set_key(FILE_PRINT_COUNT);
    s.set_value(file_print_count);
    statistics.push_back(s);

    s.set_key(BYTE_RECEIVE_COUNT);
    s.set_value(byte_receive_count);
    statistics.push_back(s);

    s.set_category(PbStatisticsCategory::CATEGORY_ERROR);

    s.set_key(PRINT_ERROR_COUNT);
    s.set_value(print_error_count);
    statistics.push_back(s);

    s.set_category(PbStatisticsCategory::CATEGORY_WARNING);

    s.set_key(PRINT_WARNING_COUNT);
    s.set_value(print_warning_count);
    statistics.push_back(s);

    return statistics;
}
