//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2026 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_display.h"
#include <map>
#include <set>
#include <spdlog/spdlog.h>
#include "protobuf/s2p_interface_util.h"
#include "shared/s2p_util.h"

using namespace s2p_interface_util;
using namespace s2p_util;

namespace
{

string DisplayParams(const PbDevice &pb_device)
{
    set<string, less<>> params;
    for (const auto& [key, value] : pb_device.params()) {
        params.insert(key + "=" + value);
    }

    return Join(params, ":");
}

string DisplayAttributes(const PbDeviceProperties &props)
{
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
        return fmt::format("Properties: {}\n", Join(properties));
    }

    return "";
}

string DisplayDefaultParameters(const PbDeviceProperties &properties)
{
    if (properties.default_params().empty()) {
        return "";
    }

    set<string, less<>> sorted_params;
    for (const auto& [key, value] : properties.default_params()) {
        sorted_params.insert(key + "=" + value);
    }

    return "Default parameters: " + Join(sorted_params, "\n                            ");
}

string DisplayBlockSizes(const PbDeviceProperties &properties)
{
    const set<uint32_t> sorted_sizes(properties.block_sizes().cbegin(), properties.block_sizes().cend());

    return fmt::format("Standard block size(s) in bytes: {}", Join(sorted_sizes));
}

string DisplayPermittedValues(const PbOperationParameter &parameter)
{
    if (!parameter.permitted_values_size()) {
        return "";
    }

    const set<string, less<>> sorted_values(parameter.permitted_values().cbegin(), parameter.permitted_values().cend());

    return fmt::format("      Permitted values: {}\n", Join(sorted_values));
}

string DisplayParameters(const PbOperationMetaData &meta_data)
{
    vector<PbOperationParameter> sorted_parameters(meta_data.parameters().cbegin(), meta_data.parameters().cend());
    ranges::sort(sorted_parameters, [](const auto &a, const auto &b) {return a.name() < b.name();});

    string s;

    for (const auto &parameter : sorted_parameters) {
        s += fmt::format("    {}: {}", parameter.name(), (parameter.is_mandatory() ? "mandatory" : "optional"));

        if (!parameter.description().empty()) {
            s += fmt::format(" ({})", parameter.description());
        }
        s += '\n';

        s += DisplayPermittedValues(parameter);

        if (!parameter.default_value().empty()) {
            s += fmt::format("      Default value: {}\n", parameter.default_value());
        }
    }

    return s;
}

}

string s2pctl_display::DisplayDevicesInfo(const PbDevicesInfo &devices_info)
{
    const vector<PbDevice> devices(devices_info.devices().cbegin(), devices_info.devices().cend());
    return ListDevices(devices);
}

string s2pctl_display::DisplayDeviceInfo(const PbDevice &pb_device)
{
    const string &type = PbDeviceType_IsValid(pb_device.type()) ? PbDeviceType_Name(pb_device.type()) : "????";

    string s = fmt::format("  {}:{}  {}  {}:{}:{}", pb_device.id(), pb_device.unit(), type, pb_device.vendor(),
        pb_device.product(), pb_device.revision());

    // Note: PiSCSI does not support this setting
    if (pb_device.scsi_level()) {
        s += fmt::format("  {}", GetScsiLevel(pb_device.scsi_level()));
    }
    else {
        s += "  -";
    }

    // There is no need to display "default"
    if (pb_device.caching_mode()) {
        string mode = PbCachingMode_Name(pb_device.caching_mode());
        ranges::replace(mode, '_', '-');
        s += fmt::format("  Caching mode: {}", mode);
    }

    if (pb_device.block_size()) {
        s += fmt::format("  {} bytes per block", pb_device.block_size());

        if (pb_device.block_count()) {
            s += fmt::format("  {} bytes capacity",
                static_cast<uint64_t>(pb_device.block_size()) * pb_device.block_count());
        }
    }

    if (!pb_device.file().name().empty()) {
        s += fmt::format("  {}", pb_device.file().name());
    }

    s += "  ";

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
        s += fmt::format("{}  ", Join(properties));
    }

    s += fmt::format("{}\n", DisplayParams(pb_device));

    return s;
}

string s2pctl_display::DisplayVersionInfo(const PbVersionInfo &version_info)
{
    string version = "Server version: " + version_info.identifier();
    if (version_info.identifier().empty() || version_info.major_version() >= 21) {
        if (version_info.major_version() == 21 && version_info.minor_version() < 12) {
            version += "RaSCSI";
        }
        else {
            version += "PiSCSI";
        }

        version += fmt::format(" {:02x}.{:02x}", version_info.major_version(), version_info.minor_version());

        if (version_info.patch_version() > 0) {
            version += fmt::format(".{}", version_info.patch_version());
        }
        else if (version_info.patch_version() == -1) {
            version += " (development version)";
        }
    }
    else {
        version += fmt::format(" {}.{}", version_info.major_version(), version_info.minor_version());
        if (version_info.patch_version() > 0) {
            version += fmt::format(".{}", version_info.patch_version());
        }
    }

    version += version_info.suffix() + '\n';

    return version;
}

string s2pctl_display::DisplayLogLevelInfo(const PbLogLevelInfo &log_level_info)
{
    string s;

    if (!log_level_info.log_levels_size()) {
        s += "  No log level settings available\n";
    }
    else {
        s += "s2p log levels, sorted by severity:\n";

        for (const auto &log_level : log_level_info.log_levels()) {
            s += fmt::format("  {}\n", log_level);
        }
    }

    s += fmt::format("Current s2p log level: {}\n", log_level_info.current_log_level());

    return s;
}

string s2pctl_display::DisplayDeviceTypesInfo(const PbDeviceTypesInfo &device_types_info)
{
    if (device_types_info.properties().empty()) {
        return "";
    }

    string s = "Supported device types and their properties:\n";

    vector<PbDeviceTypeProperties> sorted_properties(device_types_info.properties().cbegin(),
        device_types_info.properties().cend());
    ranges::sort(sorted_properties,
        [](const auto &a, const auto &b) {return PbDeviceType_Name(a.type()) < PbDeviceType_Name(b.type());});

    bool has_type = false;
    for (const auto &device_type_info : sorted_properties) {
        if (has_type) {
            s += '\n';
        }
        has_type = true;

        if (PbDeviceType_IsValid(device_type_info.type())) {
            s += fmt::format("  {}", PbDeviceType_Name(device_type_info.type()));
        }
        else {
            s += fmt::format("  ? {}", to_underlying(device_type_info.type()));
        }

        string indent = "  ";

        const PbDeviceProperties properties = device_type_info.properties();

        if (const string props = DisplayAttributes(properties); !props.empty()) {
            s += fmt::format("{}{}", indent, props);
            indent = "        ";
        }

        if (properties.supports_file()) {
            s += fmt::format("{}Image files or device files are supported", indent);
            indent = "\n        ";
        }

        if (properties.supports_params()) {
            s += fmt::format("{}Parameters are supported", indent);
            indent = "\n        ";
        }

        if (!properties.default_params().empty()) {
            s += fmt::format("{}{}", indent, DisplayDefaultParameters(properties));
            indent = "\n        ";
        }

        if (properties.block_sizes_size()) {
            s += fmt::format("{}{}", indent, DisplayBlockSizes(properties));
        }
    }

    s += '\n';

    return s;
}

string s2pctl_display::DisplayReservedIdsInfo(const PbReservedIdsInfo &reserved_ids_info)
{
    if (!reserved_ids_info.ids_size()) {
        return "";
    }

    const set<int32_t> sorted_ids(reserved_ids_info.ids().cbegin(), reserved_ids_info.ids().cend());

    return fmt::format("Reserved device IDs: {}\n", Join(sorted_ids));
}

string s2pctl_display::DisplayImageFile(const PbImageFile &image_file_info)
{
    string s = fmt::format("{}  {} byte(s)", image_file_info.name(), image_file_info.size());

    if (image_file_info.read_only()) {
        s += "  read-only";
    }

    if (image_file_info.type() != UNDEFINED) {
        s += fmt::format("  {}", PbDeviceType_Name(image_file_info.type()));
    }

    s += '\n';

    return s;
}

string s2pctl_display::DisplayImageFilesInfo(const PbImageFilesInfo &image_files_info)
{
    string s = fmt::format("Image file folder: {}\n", image_files_info.default_image_folder());
    s += fmt::format("Supported folder depth: {}\n", image_files_info.depth());

    if (!image_files_info.image_files().empty()) {
        vector<PbImageFile> image_files(image_files_info.image_files().cbegin(), image_files_info.image_files().cend());
        ranges::sort(image_files, [](const auto &a, const auto &b) {return a.name() < b.name();});

        s += "Available image files:\n";
        for (const auto &image_file : image_files) {
            s += "  ";

            s += DisplayImageFile(image_file);
        }
    }

    return s;
}

string s2pctl_display::DisplayNetworkInterfaces(const PbNetworkInterfacesInfo &network_interfaces_info)
{
    const set<string, less<>> sorted_interfaces(network_interfaces_info.name().cbegin(),
        network_interfaces_info.name().cend());

    return fmt::format("Available (up) network interfaces: {}\n", Join(sorted_interfaces));
}

string s2pctl_display::DisplayMappingInfo(const PbMappingInfo &mapping_info)
{
    string s = "Supported image file extension to device type mappings:\n";

    for (const map<string, PbDeviceType, less<>> sorted_mappings(mapping_info.mapping().cbegin(), mapping_info.mapping().cend());
        const auto& [extension, type] : sorted_mappings) {
        s += fmt::format("  {}->{}\n", extension, PbDeviceType_Name(type));
    }

    return s;
}

string s2pctl_display::DisplayStatisticsInfo(const PbStatisticsInfo &statistics_info)
{
    string s = "Statistics:\n";

    // Sort by ascending ID, LUN and key and by descending category
    vector<PbStatistics> sorted_statistics =
        { statistics_info.statistics().cbegin(), statistics_info.statistics().cend() };
    ranges::sort(sorted_statistics, [](const PbStatistics &a, const PbStatistics &b) {
        if (a.category() != b.category()) {
            return a.category() > b.category();
        }
        if (a.id() != b.id()) {
            return a.id() < b.id();
        }
        if (a.unit() != b.unit()) {
            return a.unit() < b.unit();
        }
        return a.key() < b.key();
    });

    PbStatisticsCategory prev_category = PbStatisticsCategory::CATEGORY_NONE;
    for (const auto &statistics : sorted_statistics) {
        if (statistics.category() != prev_category) {
            // Strip leading "CATEGORY_"
            s += fmt::format("  {}\n", PbStatisticsCategory_Name(statistics.category()).substr(9));
            prev_category = statistics.category();
        }

        s += fmt::format("    {}:{}  {}: {}\n", statistics.id(), statistics.unit(), statistics.key(),
            statistics.value());
    }

    return s;
}

string s2pctl_display::DisplayOperationInfo(const PbOperationInfo &operation_info)
{
    const PbOperationMetaData unknown_operation;

    // Copies result into a map sorted by operation name
    map<string, PbOperationMetaData, less<>> sorted_operations;
    for (const auto& [ordinal, meta_data] : operation_info.operations()) {
        if (PbOperation_IsValid(static_cast<PbOperation>(ordinal))) {
            sorted_operations[PbOperation_Name(static_cast<PbOperation>(ordinal))] = meta_data;
        }
        else {
            // If the server-side operation is unknown for the client use the server-provided operation name
            // No further operation information is available in this case
            sorted_operations[meta_data.server_side_name()] = unknown_operation;
        }
    }

    string s = "Operations supported by s2p server and their parameters:\n";
    for (const auto& [name, meta_data] : sorted_operations) {
        if (!meta_data.server_side_name().empty()) {
            s += fmt::format("  {}", name);
            if (!meta_data.description().empty()) {
                s += fmt::format(" ({})", meta_data.description());
            }
            s += '\n';

            s += DisplayParameters(meta_data);
        }
        else {
            s += fmt::format("  {} (Unknown server-side operation)\n", name);
        }
    }

    return s;
}

string s2pctl_display::DisplayPropertiesInfo(const PbPropertiesInfo &properties_info)
{
    const map<string, string, less<>> sorted_properties(properties_info.s2p_properties().cbegin(),
        properties_info.s2p_properties().cend());

    string s = "s2p properties:\n";

    for (const auto& [key, value] : sorted_properties) {
        s += fmt::format("  {}={}\n", key, value);
    }

    return s;
}
