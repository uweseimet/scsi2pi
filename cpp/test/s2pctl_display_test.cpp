//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
// These tests only test key aspects of the expected output, because the output may change over time.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "s2pctl/s2pctl_display.h"

using namespace s2pctl_display;

TEST(S2pCtlDisplayTest, DisplayDevicesInfo)
{
    PbDevicesInfo info;

    EXPECT_FALSE(DisplayDevicesInfo(info).empty());
}

TEST(S2pCtlDisplayTest, DisplayDeviceInfo)
{
    PbDevice device;

    EXPECT_FALSE(DisplayDeviceInfo(device).empty());

    device.set_scsi_level(5);
    device.set_caching_mode(PbCachingMode::LINUX);
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
    EXPECT_FALSE(DisplayDeviceInfo(device).empty());

    device.set_block_size(1234);
    string s = DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("1234"));

    device.set_block_count(4321);
    s = DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("5332114"));

    device.mutable_properties()->set_supports_file(true);
    auto *file = device.mutable_file();
    file->set_name("filename");
    s = DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));

    device.mutable_properties()->set_supports_params(true);
    (*device.mutable_params())["key1"] = "value1";
    s = DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key1=value1"));
    (*device.mutable_params())["key2"] = "value2";
    s = DisplayDeviceInfo(device);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key1=value1"));
    EXPECT_NE(string::npos, s.find("key2=value2"));
}

TEST(S2pCtlDisplayTest, DisplayVersionInfo)
{
    PbVersionInfo info;

    info.set_major_version(1);
    info.set_minor_version(2);
    info.set_patch_version(3);
    info.set_identifier("identifier");
    string s = DisplayVersionInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("1.2.3"));
    EXPECT_NE(string::npos, s.find("identifier"));
    EXPECT_EQ(string::npos, s.find("development"));

    info.set_patch_version(-1);
    s = DisplayVersionInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("1.2"));

    info.set_suffix("rc");
    s = DisplayVersionInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("rc"));

    info.set_major_version(21);
    info.set_minor_version(11);
    info.set_identifier("");
    s = DisplayVersionInfo(info);
    EXPECT_NE(string::npos, s.find("RaSCSI"));
    EXPECT_NE(string::npos, s.find("development"));

    info.set_major_version(22);
    s = DisplayVersionInfo(info);
    EXPECT_NE(string::npos, s.find("PiSCSI"));
    EXPECT_NE(string::npos, s.find("development"));

    info.set_patch_version(0);
    s = DisplayVersionInfo(info);
    EXPECT_EQ(string::npos, s.find("development"));
    info.set_patch_version(1);
    s = DisplayVersionInfo(info);
    EXPECT_EQ(string::npos, s.find("development"));
}

TEST(S2pCtlDisplayTest, DisplayLogLevelInfo)
{
    PbLogLevelInfo info;

    string s = DisplayLogLevelInfo(info);
    EXPECT_FALSE(s.empty());

    info.add_log_levels("test");
    s = DisplayLogLevelInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("test"));
}

TEST(S2pCtlDisplayTest, DisplayDeviceTypesInfo)
{
    PbDeviceTypesInfo info;

    int ordinal = 1;
    while (PbDeviceType_IsValid(ordinal)) {
        PbDeviceType type = UNDEFINED;
        PbDeviceType_Parse(PbDeviceType_Name(static_cast<PbDeviceType>(ordinal)), &type);

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

    const string s = DisplayDeviceTypesInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key1=value1"));
    EXPECT_NE(string::npos, s.find("key2=value2"));
}

TEST(S2pCtlDisplayTest, DisplayReservedIdsInfo)
{
    PbReservedIdsInfo info;

    string s = DisplayReservedIdsInfo(info);
    EXPECT_TRUE(s.empty());

    info.mutable_ids()->Add(5);
    s = DisplayReservedIdsInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("5"));

    info.mutable_ids()->Add(6);
    s = DisplayReservedIdsInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("5, 6"));
}

TEST(S2pCtlDisplayTest, DisplayNetworkInterfaces)
{
    PbNetworkInterfacesInfo info;

    string s = DisplayNetworkInterfaces(info);
    EXPECT_FALSE(s.empty());

    info.mutable_name()->Add("eth0");
    s = DisplayNetworkInterfaces(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("eth0"));

    info.mutable_name()->Add("wlan0");
    s = DisplayNetworkInterfaces(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("eth0, wlan0"));
}

TEST(S2pCtlDisplayTest, DisplayStatisticsInfo)
{
    PbStatisticsInfo info;

    string s = DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_EQ(string::npos, s.find("INFO"));
    EXPECT_EQ(string::npos, s.find("WARNING"));
    EXPECT_EQ(string::npos, s.find("ERROR"));
    EXPECT_EQ(string::npos, s.find("info"));
    EXPECT_EQ(string::npos, s.find("warning"));
    EXPECT_EQ(string::npos, s.find("error"));

    auto *st1 = info.add_statistics();
    st1->set_id(1);
    st1->set_unit(1);
    st1->set_category(PbStatisticsCategory::CATEGORY_INFO);
    st1->set_key("info");
    st1->set_value(1);
    s = DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_NE(string::npos, s.find("INFO"));
    EXPECT_EQ(string::npos, s.find("WARNING"));
    EXPECT_EQ(string::npos, s.find("ERROR"));
    EXPECT_NE(string::npos, s.find("info"));
    EXPECT_EQ(string::npos, s.find("warning"));
    EXPECT_EQ(string::npos, s.find("error"));
    auto *st2 = info.add_statistics();
    st2->set_id(2);
    st2->set_unit(2);
    st2->set_category(PbStatisticsCategory::CATEGORY_WARNING);
    st2->set_key("warning");
    st2->set_value(2);
    s = DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_NE(string::npos, s.find("INFO"));
    EXPECT_NE(string::npos, s.find("WARNING"));
    EXPECT_EQ(string::npos, s.find("ERROR"));
    EXPECT_NE(string::npos, s.find("info"));
    EXPECT_NE(string::npos, s.find("warning"));
    EXPECT_EQ(string::npos, s.find("error"));
    auto *st3 = info.add_statistics();
    st3->set_id(3);
    st3->set_unit(3);
    st3->set_category(PbStatisticsCategory::CATEGORY_ERROR);
    st3->set_key("error");
    st3->set_value(3);
    s = DisplayStatisticsInfo(info);
    EXPECT_NE(string::npos, s.find("Statistics:"));
    EXPECT_NE(string::npos, s.find("INFO"));
    EXPECT_NE(string::npos, s.find("WARNING"));
    EXPECT_NE(string::npos, s.find("ERROR"));
    EXPECT_NE(string::npos, s.find("info"));
    EXPECT_NE(string::npos, s.find("warning"));
    EXPECT_NE(string::npos, s.find("error"));
    auto *st4 = info.add_statistics();
    st4->set_id(4);
    st4->set_unit(4);
    st4->set_category(PbStatisticsCategory::CATEGORY_ERROR);
    st4->set_key("error");
    st4->set_value(4);
    auto *st5 = info.add_statistics();
    st5->set_id(4);
    st5->set_unit(0);
    st5->set_category(PbStatisticsCategory::CATEGORY_ERROR);
    st5->set_key("error");
    st5->set_value(5);
    s = DisplayStatisticsInfo(info);
    const int id3_lun3 = s.find("3:3");
    EXPECT_NE(string::npos, id3_lun3);
    const int id4_lun0 = s.find("4:0");
    EXPECT_NE(string::npos, id4_lun0);
    const int id4_lun4 = s.find("4:4");
    EXPECT_NE(string::npos, id4_lun4);
    EXPECT_LT(id3_lun3, id4_lun0);
    EXPECT_LT(id4_lun0, id4_lun4);
}

TEST(S2pCtlDisplayTest, DisplayImageFile)
{
    PbImageFile file;

    string s = DisplayImageFile(file);
    EXPECT_FALSE(s.empty());

    file.set_name("filename");
    s = DisplayImageFile(file);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));
    EXPECT_EQ(string::npos, s.find("read-only"));
    EXPECT_EQ(string::npos, s.find("SCHD"));

    file.set_read_only(true);
    s = DisplayImageFile(file);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));
    EXPECT_NE(string::npos, s.find("read-only"));
    EXPECT_EQ(string::npos, s.find("SCHD"));

    file.set_type(SCHD);
    s = DisplayImageFile(file);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("SCHD"));
}

TEST(S2pCtlDisplayTest, DisplayImageFilesInfo)
{
    PbImageFilesInfo info;

    string s = DisplayImageFilesInfo(info);
    EXPECT_FALSE(DisplayImageFilesInfo(info).empty());
    EXPECT_EQ(string::npos, s.find("filename"));

    PbImageFile *file = info.add_image_files();
    file->set_name("filename");
    s = DisplayImageFilesInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("filename"));
}

TEST(S2pCtlDisplayTest, DisplayMappingInfo)
{
    PbMappingInfo info;

    string s = DisplayMappingInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(string::npos, s.find("key->SCHD"));

    (*info.mutable_mapping())["key"] = SCHD;
    s = DisplayMappingInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("key->SCHD"));
}

TEST(S2pCtlDisplayTest, DisplayPropertiesInfo)
{
    PbPropertiesInfo info;

    (*info.mutable_s2p_properties())["key"] = "value";
    const string &s = DisplayPropertiesInfo(info);
    EXPECT_NE(string::npos, s.find("s2p properties"));
    EXPECT_NE(string::npos, s.find("key=value"));
}

TEST(S2pCtlDisplayTest, DisplayOperationInfo)
{
    PbOperationInfo info;

    string s = DisplayOperationInfo(info);
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
    s = DisplayOperationInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find(PbOperation_Name(NO_OPERATION)));

    meta_data.set_server_side_name("server_side_name");
    meta_data.set_description("description");
    (*info.mutable_operations())[0] = meta_data;
    s = DisplayOperationInfo(info);
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
    s = DisplayOperationInfo(info);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(string::npos, s.find("server_side_name"));
}
