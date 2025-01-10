//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// These tests only test key aspects of the expected output, because the output may change over time.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "s2pctl/s2pctl_display.h"

TEST(S2pCtlDisplayTest, DisplayDevicesInfo)
{
    S2pCtlDisplay display;
    PbDevicesInfo info;

    EXPECT_FALSE(display.DisplayDevicesInfo(info).empty());
}

TEST(S2pCtlDisplayTest, DisplayDeviceInfo)
{
    S2pCtlDisplay display;
    PbDevice device;

    EXPECT_FALSE(display.DisplayDeviceInfo(device).empty());

    device.mutable_properties()->set_supports_file(true);
    device.mutable_properties()->set_read_only(true);
    device.mutable_properties()->set_protectable(true);
    device.mutable_status()->set_protected_(true);
    device.mutable_properties()->set_stoppable(true);
    device.mutable_status()->set_stopped(true);
    device.mutable_properties()->set_removable(true);
    device.mutable_status()->set_removed(true);
    device.mutable_properties()->set_lockable(true);
    device.mutable_status()->set_locked(true);
    EXPECT_FALSE(display.DisplayDeviceInfo(device).empty());

    device.set_block_size(1234);
    string s = display.DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("1234"));

    device.set_block_count(4321);
    s = display.DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("5332114"));

    device.mutable_properties()->set_supports_file(true);
    auto *file = device.mutable_file();
    file->set_name("filename");
    s = display.DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));

    device.mutable_properties()->set_supports_params(true);
    (*device.mutable_params())["key1"] = "value1";
    s = display.DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key1=value1"));
    (*device.mutable_params())["key2"] = "value2";
    s = display.DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key1=value1"));
    EXPECT_NE(string::npos, s.find("key2=value2"));
}

TEST(S2pCtlDisplayTest, DisplayVersionInfo)
{
    S2pCtlDisplay display;
    PbVersionInfo info;

    info.set_major_version(1);
    info.set_minor_version(2);
    info.set_patch_version(3);
    info.set_identifier("identifier");
    string s = display.DisplayVersionInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("1.2.3"));
    EXPECT_NE(string::npos, s.find("identifier"));
    EXPECT_EQ(string::npos, s.find("development"));

    info.set_patch_version(-1);
    s = display.DisplayVersionInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("1.2"));

    info.set_suffix("rc");
    s = display.DisplayVersionInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("rc"));

    info.set_major_version(21);
    info.set_minor_version(11);
    info.set_identifier("");
    s = display.DisplayVersionInfo(info);
    EXPECT_NE(string::npos, s.find("RaSCSI"));
    EXPECT_NE(string::npos, s.find("development"));

    info.set_major_version(22);
    s = display.DisplayVersionInfo(info);
    EXPECT_NE(string::npos, s.find("PiSCSI"));
    EXPECT_NE(string::npos, s.find("development"));

    info.set_patch_version(0);
    s = display.DisplayVersionInfo(info);
    EXPECT_EQ(string::npos, s.find("development"));
    info.set_patch_version(1);
    s = display.DisplayVersionInfo(info);
    EXPECT_EQ(string::npos, s.find("development"));
}

TEST(S2pCtlDisplayTest, DisplayLogLevelInfo)
{
    S2pCtlDisplay display;
    PbLogLevelInfo info;

    string s = display.DisplayLogLevelInfo(info);
    EXPECT_FALSE(s.empty());

    info.add_log_levels("test");
    s = display.DisplayLogLevelInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("test"));
}

TEST(S2pCtlDisplayTest, DisplayDeviceTypesInfo)
{
    S2pCtlDisplay display;
    PbDeviceTypesInfo info;

    int ordinal = 1;
    while (PbDeviceType_IsValid(ordinal)) {
        PbDeviceType type = UNDEFINED;
        PbDeviceType_Parse(PbDeviceType_Name((PbDeviceType)ordinal), &type);

        auto *type_properties = info.add_properties();
        type_properties->set_type(type);

        if (type == SCHD) {
            type_properties->mutable_properties()->set_supports_file(true);
            type_properties->mutable_properties()->add_block_sizes(512);
            type_properties->mutable_properties()->add_block_sizes(1024);
        }

        if (type == SCMO) {
            type_properties->mutable_properties()->set_supports_file(true);
            type_properties->mutable_properties()->set_read_only(true);
            type_properties->mutable_properties()->set_protectable(true);
            type_properties->mutable_properties()->set_stoppable(true);
            type_properties->mutable_properties()->set_removable(true);
            type_properties->mutable_properties()->set_lockable(true);
        }

        if (type == SCLP) {
            type_properties->mutable_properties()->set_supports_params(true);
            (*type_properties->mutable_properties()->mutable_default_params())["key1"] = "value1";
            (*type_properties->mutable_properties()->mutable_default_params())["key2"] = "value2";
        }

        ++ordinal;
    }

    const string s = display.DisplayDeviceTypesInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key1=value1"));
    EXPECT_NE(string::npos, s.find("key2=value2"));
}

TEST(S2pCtlDisplayTest, DisplayReservedIdsInfo)
{
    S2pCtlDisplay display;
    PbReservedIdsInfo info;

    string s = display.DisplayReservedIdsInfo(info);
    EXPECT_TRUE(s.empty());

    info.mutable_ids()->Add(5);
    s = display.DisplayReservedIdsInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("5"));

    info.mutable_ids()->Add(6);
    s = display.DisplayReservedIdsInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("5, 6"));
}

TEST(S2pCtlDisplayTest, DisplayNetworkInterfaces)
{
    S2pCtlDisplay display;
    PbNetworkInterfacesInfo info;

    string s = display.DisplayNetworkInterfaces(info);
    EXPECT_FALSE(s.empty());

    info.mutable_name()->Add("eth0");
    s = display.DisplayNetworkInterfaces(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("eth0"));

    info.mutable_name()->Add("wlan0");
    s = display.DisplayNetworkInterfaces(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("eth0, wlan0"));
}

TEST(S2pCtlDisplayTest, DisplayStatisticsInfo)
{
    S2pCtlDisplay display;
    PbStatisticsInfo info;

    string s = display.DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_EQ(string::npos, s.find("INFO"));
    EXPECT_EQ(string::npos, s.find("WARNING"));
    EXPECT_EQ(string::npos, s.find("ERROR"));
    EXPECT_EQ(string::npos, s.find("info"));
    EXPECT_EQ(string::npos, s.find("warning"));
    EXPECT_EQ(string::npos, s.find("error"));

    auto *st1 = info.add_statistics();
    st1->set_category(PbStatisticsCategory::CATEGORY_INFO);
    st1->set_key("info");
    st1->set_value(1);
    s = display.DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_NE(string::npos, s.find("INFO"));
    EXPECT_EQ(string::npos, s.find("WARNING"));
    EXPECT_EQ(string::npos, s.find("ERROR"));
    EXPECT_NE(string::npos, s.find("info"));
    EXPECT_EQ(string::npos, s.find("warning"));
    EXPECT_EQ(string::npos, s.find("error"));
    auto *st2 = info.add_statistics();
    st2->set_category(PbStatisticsCategory::CATEGORY_WARNING);
    st2->set_key("warning");
    st2->set_value(2);
    s = display.DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_NE(string::npos, s.find("INFO"));
    EXPECT_NE(string::npos, s.find("WARNING"));
    EXPECT_EQ(string::npos, s.find("ERROR"));
    EXPECT_NE(string::npos, s.find("info"));
    EXPECT_NE(string::npos, s.find("warning"));
    EXPECT_EQ(string::npos, s.find("error"));
    auto *st3 = info.add_statistics();
    st3->set_category(PbStatisticsCategory::CATEGORY_ERROR);
    st3->set_key("error");
    st3->set_value(3);
    s = display.DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_NE(string::npos, s.find("INFO"));
    EXPECT_NE(string::npos, s.find("WARNING"));
    EXPECT_NE(string::npos, s.find("ERROR"));
    EXPECT_NE(string::npos, s.find("info"));
    EXPECT_NE(string::npos, s.find("warning"));
    EXPECT_NE(string::npos, s.find("error"));
}

TEST(S2pCtlDisplayTest, DisplayImageFile)
{
    S2pCtlDisplay display;
    PbImageFile file;

    string s = display.DisplayImageFile(file);
    EXPECT_FALSE(s.empty());

    file.set_name("filename");
    s = display.DisplayImageFile(file);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));
    EXPECT_EQ(string::npos, s.find("read-only"));
    EXPECT_EQ(string::npos, s.find("SCHD"));

    file.set_read_only(true);
    s = display.DisplayImageFile(file);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));
    EXPECT_NE(string::npos, s.find("read-only"));
    EXPECT_EQ(string::npos, s.find("SCHD"));

    file.set_type(SCHD);
    s = display.DisplayImageFile(file);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("SCHD"));
}

TEST(S2pCtlDisplayTest, DisplayImageFilesInfo)
{
    S2pCtlDisplay display;
    PbImageFilesInfo info;

    string s = display.DisplayImageFilesInfo(info);
    EXPECT_FALSE(display.DisplayImageFilesInfo(info).empty());
    EXPECT_EQ(string::npos, s.find("filename"));

    PbImageFile *file = info.add_image_files();
    file->set_name("filename");
    s = display.DisplayImageFilesInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));
}

TEST(S2pCtlDisplayTest, DisplayMappingInfo)
{
    S2pCtlDisplay display;
    PbMappingInfo info;

    string s = display.DisplayMappingInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(string::npos, s.find("key->SCHD"));

    (*info.mutable_mapping())["key"] = SCHD;
    s = display.DisplayMappingInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key->SCHD"));
}

TEST(S2pCtlDisplayTest, DisplayPropertiesInfo)
{
    S2pCtlDisplay display;
    PbPropertiesInfo info;

    const string s = display.DisplayPropertiesInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("s2p properties"));
}

TEST(S2pCtlDisplayTest, DisplayOperationInfo)
{
    S2pCtlDisplay display;
    PbOperationInfo info;

    string s = display.DisplayOperationInfo(info);
    EXPECT_FALSE(s.empty());

    PbOperationMetaData meta_data;
    PbOperationParameter *param1 = meta_data.add_parameters();
    param1->set_name("default_key1");
    param1->set_default_value("default_value1");
    PbOperationParameter *param2 = meta_data.add_parameters();
    param2->set_name("default_key2");
    param2->set_default_value("default_value2");
    param2->set_description("description2");
    PbOperationParameter *param3 = meta_data.add_parameters();
    param3->set_name("default_key3");
    param3->set_default_value("default_value3");
    param3->set_description("description3");
    param3->add_permitted_values("permitted_value3_1");
    param3->add_permitted_values("permitted_value3_2");
    (*info.mutable_operations())[0] = meta_data;
    s = display.DisplayOperationInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find(PbOperation_Name(NO_OPERATION)));

    meta_data.set_server_side_name("server_side_name");
    meta_data.set_description("description");
    (*info.mutable_operations())[0] = meta_data;
    s = display.DisplayOperationInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("default_key1"));
    EXPECT_NE(string::npos, s.find("default_value1"));
    EXPECT_NE(string::npos, s.find("default_key2"));
    EXPECT_NE(string::npos, s.find("default_value2"));
    EXPECT_NE(string::npos, s.find("description2"));
    EXPECT_NE(string::npos, s.find("description3"));
    EXPECT_NE(string::npos, s.find("permitted_value3_1"));
    EXPECT_NE(string::npos, s.find("permitted_value3_2"));
    EXPECT_EQ(string::npos, s.find("server_side_name"));

    (*info.mutable_operations())[1234] = meta_data;
    s = display.DisplayOperationInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("server_side_name"));
}
