//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_display.h"
#include <iomanip>
#include <map>
#include <set>
#include <spdlog/spdlog.h>
#include "protobuf/protobuf_util.h"
#include "shared/s2p_util.h"

using namespace s2p_util;
using namespace protobuf_util;

string S2pCtlDisplay::DisplayDevicesInfo(const PbDevicesInfo &devices_info) const
{
    const vector<PbDevice> devices(devices_info.devices().cbegin(), devices_info.devices().cend());

    return ListDevices(devices);
}

string S2pCtlDisplay::DisplayDeviceInfo(const PbDevice &pb_device) const
{
    ostringstream s;

    const string &type = PbDeviceType_IsValid(pb_device.type()) ? PbDeviceType_Name(pb_device.type()) : "????";

    s << "  " << pb_device.id() << ":" << pb_device.unit() << "  " << type
        << "  " << pb_device.vendor() << ":" << pb_device.product() << ":" << pb_device.revision();

    // Check for existence because PiSCSI does not support this setting
    if (pb_device.scsi_level()) {
        s << "  " << GetScsiLevel(pb_device.scsi_level());
    }

    // There is no need to display "default"
    if (pb_device.caching_mode()) {
        string mode = PbCachingMode_Name(pb_device.caching_mode());
        ranges::replace(mode, '_', '-');
        s << "  Caching mode: " << mode;
    }

    if (pb_device.block_size()) {
        s << "  " << pb_device.block_size() << " bytes per block";

        if (pb_device.block_count()) {
            s << "  " << pb_device.block_size() * pb_device.block_count() << " bytes capacity";
        }
    }

    if (!pb_device.file().name().empty()) {
        s << "  " << pb_device.file().name();
    }

    s << "  ";

    vector<string> properties;

    if (pb_device.properties().read_only()) {
        properties.emplace_back("read-only");
    }

    if (pb_device.properties().protectable() && pb_device.status().protected_()) {
        properties.emplace_back("protected");
    }

    if (pb_device.properties().stoppable() && pb_device.status().stopped()) {
        properties.emplace_back("stopped");
    }

    if (pb_device.properties().removable() && pb_device.status().removed()) {
        properties.emplace_back("removed");
    }

    if (pb_device.properties().lockable() && pb_device.status().locked()) {
        properties.emplace_back("locked");
    }

    if (!properties.empty()) {
        s << Join(properties) << "  ";
    }

    s << DisplayParams(pb_device) << '\n';

    return s.str();
}

string S2pCtlDisplay::DisplayVersionInfo(const PbVersionInfo &version_info) const
{
    string version = "Server version: " + version_info.identifier();
    if (version_info.identifier().empty() || version_info.major_version() >= 21) {
        if (version_info.major_version() == 21 && version_info.minor_version() < 12) {
            version += "RaSCSI";
        }
        else {
            version += "PiSCSI";
        }

        version += fmt::format(" {0:02x}.{1:02x}", version_info.major_version(), version_info.minor_version());

        if (version_info.patch_version() > 0) {
            version += fmt::format(".{}", version_info.patch_version());
        }
        else if (version_info.patch_version() == -1) {
            version += " (development version)";
        }
    }
    else {
        version += fmt::format(" {0}.{1}", version_info.major_version(), version_info.minor_version());
        if (version_info.patch_version() > 0) {
            version += fmt::format(".{}", version_info.patch_version());
        }
    }

    version += version_info.suffix() + '\n';

    return version;
}

string S2pCtlDisplay::DisplayLogLevelInfo(const PbLogLevelInfo &log_level_info) const
{
    ostringstream s;

    if (!log_level_info.log_levels_size()) {
        s << "  No log level settings available\n";
    }
    else {
        s << "s2p log levels, sorted by severity:\n";

        for (const auto &log_level : log_level_info.log_levels()) {
            s << "  " << log_level << '\n';
        }
    }

    s << "Current s2p log level: " << log_level_info.current_log_level() << '\n';

    return s.str();
}

string S2pCtlDisplay::DisplayDeviceTypesInfo(const PbDeviceTypesInfo &device_types_info) const
{
    ostringstream s;

    if (device_types_info.properties().empty()) {
        return "";
    }

    s << "Supported device types and their properties:\n";

    vector<PbDeviceTypeProperties> sorted_properties(device_types_info.properties().cbegin(),
        device_types_info.properties().cend());
    ranges::sort(sorted_properties,
        [](const auto &a, const auto &b) {return PbDeviceType_Name(a.type()) < PbDeviceType_Name(b.type());});

    bool has_type = false;
    string indent;
    for (const auto &device_type_info : sorted_properties) {
        if (has_type) {
            s << '\n';
        }
        has_type = true;

        s << "  " << PbDeviceType_Name(device_type_info.type());

        indent = "  ";

        const PbDeviceProperties &properties = device_type_info.properties();

        if (const string &props = DisplayAttributes(properties); !props.empty()) {
            s << indent << props;
            indent = "        ";
        }

        if (properties.supports_file()) {
            s << indent << "Image files are supported";
            indent = "\n        ";
        }

        if (properties.supports_params()) {
            s << indent << "Parameters are supported";
            indent = "\n        ";
        }

        if (!properties.default_params().empty()) {
            s << indent << DisplayDefaultParameters(properties);
            indent = "\n        ";
        }

        if (properties.block_sizes_size()) {
            s << indent << DisplayBlockSizes(properties);
        }
    }

    s << '\n';

    return s.str();
}

string S2pCtlDisplay::DisplayReservedIdsInfo(const PbReservedIdsInfo &reserved_ids_info) const
{
    ostringstream s;

    if (reserved_ids_info.ids_size()) {
        const set<int32_t> sorted_ids(reserved_ids_info.ids().cbegin(), reserved_ids_info.ids().cend());
        s << "Reserved device IDs: " << Join(sorted_ids) << '\n';
    }

    return s.str();
}

string S2pCtlDisplay::DisplayImageFile(const PbImageFile &image_file_info) const
{
    ostringstream s;

    s << image_file_info.name() << "  " << image_file_info.size() << " bytes";

    if (image_file_info.read_only()) {
        s << "  read-only";
    }

    if (image_file_info.type() != UNDEFINED) {
        s << "  " << PbDeviceType_Name(image_file_info.type());
    }

    s << '\n';

    return s.str();
}

string S2pCtlDisplay::DisplayImageFilesInfo(const PbImageFilesInfo &image_files_info) const
{
    ostringstream s;

    s << "Default image file folder: " << image_files_info.default_image_folder() << '\n';
    s << "Supported folder depth: " << image_files_info.depth() << '\n';

    if (!image_files_info.image_files().empty()) {
        vector<PbImageFile> image_files(image_files_info.image_files().cbegin(), image_files_info.image_files().cend());
        ranges::sort(image_files, [](const auto &a, const auto &b) {return a.name() < b.name();});

        s << "Available image files:\n";
        for (const auto &image_file : image_files) {
            s << "  ";

            s << DisplayImageFile(image_file);
        }
    }

    return s.str();
}

string S2pCtlDisplay::DisplayNetworkInterfaces(const PbNetworkInterfacesInfo &network_interfaces_info) const
{
    ostringstream s;

    const set<string, less<>> sorted_interfaces(network_interfaces_info.name().cbegin(),
        network_interfaces_info.name().cend());
    s << "Available (up) network interfaces: " << Join(sorted_interfaces) << '\n';

    return s.str();
}

string S2pCtlDisplay::DisplayMappingInfo(const PbMappingInfo &mapping_info) const
{
    ostringstream s;

    s << "Supported image file extension to device type mappings:\n";

    for (const map<string, PbDeviceType, less<>> sorted_mappings(mapping_info.mapping().cbegin(), mapping_info.mapping().cend());
        const auto& [extension, type] : sorted_mappings) {
        s << "  " << extension << "->" << PbDeviceType_Name(type) << '\n';
    }

    return s.str();
}

string S2pCtlDisplay::DisplayStatisticsInfo(const PbStatisticsInfo &statistics_info) const
{
    ostringstream s;

    s << "Statistics:\n";

    // Sort by ascending ID, LUN and key and by descending category
    vector<PbStatistics> sorted_statistics =
        { statistics_info.statistics().cbegin(), statistics_info.statistics().cend() };
    ranges::sort(sorted_statistics, [](const PbStatistics &a, const PbStatistics &b) {
        if (a.category() > b.category()) return true;
        if (a.category() < b.category()) return false;
        if (a.id() < b.id()) return true;
        if (a.id() > b.id()) return false;
        if (a.unit() < b.unit()) return true;
        if (a.unit() > b.unit()) return false;
        return a.key() < b.key();
    });

    PbStatisticsCategory prev_category = PbStatisticsCategory::CATEGORY_NONE;
    for (const auto &statistics : sorted_statistics) {
        if (statistics.category() != prev_category) {
            // Strip leading "CATEGORY_"
            s << "  " << PbStatisticsCategory_Name(statistics.category()).substr(9) << '\n';
            prev_category = statistics.category();
        }

        s << "    " << statistics.id() << ":" << statistics.unit() << "  " << statistics.key() << ": "
            << statistics.value() << '\n';
    }

    return s.str();
}

string S2pCtlDisplay::DisplayOperationInfo(const PbOperationInfo &operation_info) const
{
    ostringstream s;

    const map<int, PbOperationMetaData, less<>> operations(operation_info.operations().cbegin(),
        operation_info.operations().cend());

    // Copies result into a map sorted by operation name
    auto unknown_operation = make_unique<PbOperationMetaData>();
    map<string, PbOperationMetaData, less<>> sorted_operations;

    for (const auto& [ordinal, meta_data] : operations) {
        if (PbOperation_IsValid(static_cast<PbOperation>(ordinal))) {
            sorted_operations[PbOperation_Name(static_cast<PbOperation>(ordinal))] = meta_data;
        }
        else {
            // If the server-side operation is unknown for the client use the server-provided operation name
            // No further operation information is available in this case
            sorted_operations[meta_data.server_side_name()] = *unknown_operation;
        }
    }

    s << "Operations supported by s2p server and their parameters:\n";
    for (const auto& [name, meta_data] : sorted_operations) {
        if (!meta_data.server_side_name().empty()) {
            s << "  " << name;
            if (!meta_data.description().empty()) {
                s << " (" << meta_data.description() << ")";
            }
            s << '\n';

            s << DisplayParameters(meta_data);
        }
        else {
            s << "  " << name << " (Unknown server-side operation)\n";
        }
    }

    return s.str();
}

string S2pCtlDisplay::DisplayPropertiesInfo(const PbPropertiesInfo &properties_info) const
{
    ostringstream s;

    const map<string, string, less<>> sorted_properties(properties_info.s2p_properties().cbegin(),
        properties_info.s2p_properties().cend());

    s << "s2p properties:\n";
    for (const auto& [key, value] : sorted_properties) {
        s << "  " << key << "=" << value << '\n';
    }

    return s.str();
}

string S2pCtlDisplay::DisplayParams(const PbDevice &pb_device) const
{
    set<string, less<>> params;
    for (const auto& [key, value] : pb_device.params()) {
        params.insert(key + "=" + value);
    }

    return Join(params, ":");
}

string S2pCtlDisplay::DisplayAttributes(const PbDeviceProperties &props) const
{
    ostringstream s;

    vector<string> properties;
    if (props.read_only()) {
        properties.emplace_back("read-only");
    }
    if (props.protectable()) {
        properties.emplace_back("protectable");
    }
    if (props.stoppable()) {
        properties.emplace_back("stoppable");
    }
    if (props.removable()) {
        properties.emplace_back("removable");
    }
    if (props.lockable()) {
        properties.emplace_back("lockable");
    }

    if (!properties.empty()) {
        s << "Properties: " << Join(properties) << '\n';
    }

    return s.str();
}

string S2pCtlDisplay::DisplayDefaultParameters(const PbDeviceProperties &properties) const
{
    ostringstream s;

    if (!properties.default_params().empty()) {
        s << "Default parameters: ";

        set<string, less<>> sorted_params;
        for (const auto& [key, value] : properties.default_params()) {
            sorted_params.insert(key + "=" + value);
        }

        string p;

        for (const auto &param : sorted_params) {
            if (!p.empty()) {
                p += "\n                            ";
            }
            p += param;
        }

        s << p;
    }

    return s.str();
}

string S2pCtlDisplay::DisplayBlockSizes(const PbDeviceProperties &properties) const
{
    ostringstream s;

    if (properties.block_sizes_size()) {
        const set<uint32_t> sorted_sizes(properties.block_sizes().cbegin(), properties.block_sizes().cend());
        s << "Standard block size" << (sorted_sizes.size() > 1 ? "s" : "") << " in bytes: "
            << Join(sorted_sizes);
    }

    return s.str();
}

string S2pCtlDisplay::DisplayParameters(const PbOperationMetaData &meta_data) const
{
    vector<PbOperationParameter> sorted_parameters(meta_data.parameters().cbegin(), meta_data.parameters().cend());
    ranges::sort(sorted_parameters, [](const auto &a, const auto &b) {return a.name() < b.name();});

    ostringstream s;

    for (const auto &parameter : sorted_parameters) {
        s << "    " << parameter.name() << ": "
            << (parameter.is_mandatory() ? "mandatory" : "optional");

        if (!parameter.description().empty()) {
            s << " (" << parameter.description() << ")";
        }
        s << '\n';

        s << DisplayPermittedValues(parameter);

        if (!parameter.default_value().empty()) {
            s << "      Default value: " << parameter.default_value() << '\n';
        }
    }

    return s.str();
}

string S2pCtlDisplay::DisplayPermittedValues(const PbOperationParameter &parameter) const
{
    ostringstream s;
    if (parameter.permitted_values_size()) {
        const set<string, less<>> sorted_values(parameter.permitted_values().cbegin(),
            parameter.permitted_values().cend());
        s << "      Permitted values: " << Join(sorted_values) << '\n';
    }

    return s.str();
}
