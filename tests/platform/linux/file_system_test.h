#ifndef __WBE_FILE_SYSTEM_TEST_H__
#define __WBE_FILE_SYSTEM_TEST_H__

#include "platform/file_system/directory.h"
#include "platform/file_system/file_system.h"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(LinuxFileSystemTest, DirEquals) {
    WBE::Directory dir1({
        "Hello",
        "what",
        "is",
        "up"
    }, true);
    ASSERT_TRUE(dir1 == dir1);
    ASSERT_FALSE(dir1 != dir1);
    WBE::Directory dir2({
        "Hello",
        "what",
        "is",
        "up"
    }, false);
    ASSERT_FALSE(dir1 == dir2);
    ASSERT_TRUE(dir1 != dir2);
    WBE::Directory dir3({
        "Hello",
        "what",
        "is"
    }, true);
    ASSERT_FALSE(dir1 == dir2);
    ASSERT_TRUE(dir1 != dir2);
}

TEST(LinuxFileSystemTest, DirToString) {
    WBE::Directory test_dir({
        "hello",
        "world",
        "this",
        "is",
        "a",
        "test",
        "directory"
    }, false);
    ASSERT_EQ(WBE::FileSystem::dir_to_string(test_dir), std::string("hello/world/this/is/a/test/directory/"));
    ASSERT_EQ(std::string(test_dir), std::string("hello/world/this/is/a/test/directory/"));
    WBE::Directory test_dir_absolute({
        "this",
        "is",
        "a",
        "absolute",
        "test",
        "directory"
    }, true);
    ASSERT_EQ(WBE::FileSystem::dir_to_string(test_dir_absolute), std::string("/this/is/a/absolute/test/directory/"));
    ASSERT_EQ(std::string(test_dir_absolute), std::string("/this/is/a/absolute/test/directory/"));
}

TEST(LinuxFileSystemTest, ParseDirectory) {
    WBE::Directory expected({
        "hello",
        "world",
        "this",
        "is",
        "a",
        "test",
        "directory"
    }, false);
    WBE::Directory parsed_dir = WBE::FileSystem::parse_directory("hello/world//this/is/not/../a/test/./directory/XD");
    ASSERT_TRUE(parsed_dir == expected);
    WBE::Directory parsed_dir_1 = WBE::FileSystem::parse_directory("../hello/world/this/is/not/../a/test/./directory/XD/../");
    ASSERT_TRUE(parsed_dir_1 == expected);
    WBE::Directory parsed_dir_2 = WBE::FileSystem::parse_directory("./hello/world/this/is/not/../a/test/./directory/XD/../");
    ASSERT_TRUE(parsed_dir_2 == expected);
    WBE::Directory expected_absolute({
        "hello",
        "this",
        "is",
        "me"
    }, true);
    WBE::Directory parsed_dir_absolute = WBE::FileSystem::parse_directory("/../hello/this/is/me/");
    ASSERT_TRUE(parsed_dir_absolute == expected_absolute);
    WBE::Directory parsed_dir_absolute_1 = WBE::FileSystem::parse_directory("/./../hello/that/../this/is/me/XD");
    ASSERT_TRUE(parsed_dir_absolute_1 == expected_absolute);
    WBE::Directory parsed_dir_absolute_2 = WBE::FileSystem::parse_directory("/../hello/this/is/me/");
    ASSERT_TRUE(parsed_dir_absolute_2 == expected_absolute);
}

#endif
