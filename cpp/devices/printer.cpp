//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2026 Uwe Seimet
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
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace memory_util;

Printer::Printer(int lun) : PrimaryDevice(SCLP, lun)
{
    SetProductData( { "", "SCSI PRINTER", "" }, true);
    SetScsiLevel(ScsiLevel::SCSI_2);
    SupportsParams(true);
    SetReady(true);
}

string Printer::SetUp()
{
    if (GetParam(CMD).find("%f") == string::npos) {
        return "Missing filename specifier '%f'";
    }

    AddCommand(ScsiCommand::PRINT, [this]
        {
            Print();
        });
    AddCommand(ScsiCommand::SYNCHRONIZE_BUFFER, [this]
        {
            SynchronizeBuffer();
        });
    AddCommand(ScsiCommand::STOP_PRINT, [this]
        {
            StatusPhase();
        });

    error_code error;
    file_template = temp_directory_path(error); // NOSONAR Publicly writable directory is safe here
    file_template += PRINTER_FILE_PATTERN;

    return "";
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
    return HandleInquiry(DeviceType::PRINTER);
}

void Printer::Print()
{
    const uint32_t length = GetCdbInt24(2);

    LogTrace(fmt::format("Expecting to receive {} byte(s) for printing", length));

    if (length > GetController()->GetBuffer().size()) {
        LogError(fmt::format("Transfer buffer overflow: Buffer size is {0} bytes, {1} byte(s) expected",
            GetController()->GetBuffer().size(), length));

        ++print_error_count;

        throw ScsiException(SenseKey::ILLEGAL_REQUEST, Asc::INVALID_FIELD_IN_CDB);
    }

    GetController()->SetTransferSize(length, length);

    DataOutPhase(length);
}

void Printer::SynchronizeBuffer()
{
    if (!out.is_open()) {
        LogWarn("Nothing to print");

        ++print_warning_count;

        throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);
    }

    out.close();

    string cmd = GetParam(CMD);
    const size_t file_position = cmd.find("%f");
    // The format has been verified before
    assert(file_position != string::npos);
    cmd.replace(file_position, 2, filename);

    error_code error;
    LogTrace(fmt::format("Printing file '{0}' with {1} byte(s) using print command '{2}'", filename,
        file_size(path(filename), error), cmd));

    if (system(cmd.c_str())) {
        LogError(fmt::format("Printing file '{}' failed, the Pi's printing system might not be configured", filename));

        ++print_error_count;

        CleanUp();

        throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);
    }

    CleanUp();

    StatusPhase();
}

int Printer::WriteData(cdb_t cdb, data_out_t buf, int l)
{
    if (cdb[0] != static_cast<int>(ScsiCommand::PRINT)) {
        throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::INTERNAL_TARGET_FAILURE);
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
            throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);
        }
        close(fd);

        filename = f.data();

        out.open(filename, ios::binary);
        CheckForFileError();
    }

    LogTrace(fmt::format("Appending {0} byte(s) to printer output file '{1}'", length, filename));

    out.write(reinterpret_cast<const char*>(buf.data()), length);
    CheckForFileError();

    return l;
}

void Printer::CheckForFileError()
{
    if (out.fail()) {
        out.clear();
        ++print_error_count;
        throw ScsiException(SenseKey::ABORTED_COMMAND, Asc::IO_PROCESS_TERMINATED);
    }
}

vector<PbStatistics> Printer::GetStatistics() const
{
    vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

    EnrichStatistics(statistics, CATEGORY_INFO, FILE_PRINT_COUNT, file_print_count);
    EnrichStatistics(statistics, CATEGORY_INFO, BYTE_RECEIVE_COUNT, byte_receive_count);
    EnrichStatistics(statistics, CATEGORY_ERROR, PRINT_ERROR_COUNT, print_error_count);
    EnrichStatistics(statistics, CATEGORY_WARNING, PRINT_WARNING_COUNT, print_warning_count);

    return statistics;
}
