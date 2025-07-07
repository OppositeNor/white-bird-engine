#ifndef __WBE_FILE_SYSTEM_TEST_H__
#define __WBE_FILE_SYSTEM_TEST_H__

#include "platform\\file_system\\directory.h"
#include "platform\\file_system\\file_system.h"
#include <gtest\\gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(MacOSFileSystemTest, DirEquals) {
    WBE::Directory dir1({ "Hello", "what", "is", "up" }, true);
    ASSERT_TRUE(dir1 == dir1);
    ASSERT_FALSE(dir1 != dir1);
    WBE::Directory dir2({ "Hello", "what", "is", "up" }, false);
    ASSERT_FALSE(dir1 == dir2);
    ASSERT_TRUE(dir1 != dir2);
    WBE::Directory dir3({ "Hello", "what", "is" }, true);
    ASSERT_FALSE(dir1 == dir2);
    ASSERT_TRUE(dir1 != dir2);
    WBE::Directory dir_empty1({}, false);
    WBE::Directory dir_empty2({}, false);
    WBE::Directory dir_empty3({}, true);
    WBE::Directory dir_empty4({}, true);
    ASSERT_TRUE(dir_empty1 == dir_empty2);
    ASSERT_TRUE(dir_empty3 == dir_empty4);
    ASSERT_TRUE(dir_empty1 != dir_empty3);
    ASSERT_TRUE(dir_empty3 != dir_empty1);
    ASSERT_FALSE(dir_empty1 == dir_empty4);
    ASSERT_FALSE(dir_empty4 == dir_empty1);
    ASSERT_FALSE(dir_empty1 != dir_empty2);
    ASSERT_FALSE(dir_empty3 != dir_empty4);
}

TEST(MacOSFileSystemTest, DirToString) {
    WBE::Directory test_dir({ "hello", "world", "this", "is", "a", "test", "directory" }, false);
    ASSERT_EQ(WBE::FileSystem::dir_to_string(test_dir), std::string("hello\\world\\this\\is\\a\\test\\directory\\"));
    ASSERT_EQ(std::string(test_dir), std::string("hello\\world\\this\\is\\a\\test\\directory\\"));
    WBE::Directory test_dir_absolute({ "C", "this", "is", "a", "absolute", "test", "directory" }, true);
    ASSERT_EQ(WBE::FileSystem::dir_to_string(test_dir_absolute), std::string("\\this\\is\\a\\absolute\\test\\directory\\"));
    ASSERT_EQ(std::string(test_dir_absolute), std::string("C:\\this\\is\\a\\absolute\\test\\directory\\"));
}

TEST(MacOSFileSystemTest, ParseDirectory) {
    WBE::Directory expected({ "hello", "world", "this", "is", "a", "test", "directory" }, false);
    WBE::Directory parsed_dir = WBE::FileSystem::parse_directory("hello\\world\\\\this\\is\\not\\..\\a\\test\\.\\directory\\XD");
    ASSERT_TRUE(parsed_dir == expected);
    WBE::Directory parsed_dir_1 = WBE::FileSystem::parse_directory("..\\hello\\world\\this\\\\\\is\\not\\..\\a\\test\\.\\directory\\XD\\..\\");
    ASSERT_TRUE(parsed_dir_1 == expected);
    WBE::Directory parsed_dir_2 = WBE::FileSystem::parse_directory(".\\hello\\world\\this\\is\\not\\\\..\\a\\test\\.\\directory\\XD\\..\\");
    ASSERT_TRUE(parsed_dir_2 == expected);
    WBE::Directory expected_absolute({ "D:", "hello", "this", "is", "me" }, true);
    WBE::Directory parsed_dir_absolute = WBE::FileSystem::parse_directory("D:\\..\\hello\\this\\is\\me\\");
    ASSERT_TRUE(parsed_dir_absolute == expected_absolute);
    WBE::Directory parsed_dir_absolute_1 = WBE::FileSystem::parse_directory("D:\\.\\..\\hello\\that\\..\\\\this\\\\is\\\\\\\\\\me\\XD\\");
    ASSERT_TRUE(parsed_dir_absolute_1 == expected_absolute);
    WBE::Directory parsed_dir_absolute_2 = WBE::FileSystem::parse_directory("D:\\..\\hello\\this\\is\\me\\\\");
    ASSERT_TRUE(parsed_dir_absolute_2 == expected_absolute);
    WBE::Directory expectd_relative_empty({}, false);
    WBE::Directory expectd_absolute_empty({"E:"}, true);
    WBE::Directory parsed_dir_relative_empty = WBE::FileSystem::parse_directory("");
    WBE::Directory parsed_dir_absolute_empty = WBE::FileSystem::parse_directory("E:\\");
    ASSERT_TRUE(parsed_dir_relative_empty == expectd_relative_empty);
    ASSERT_TRUE(parsed_dir_absolute_empty == expectd_absolute_empty);
}

TEST(MacOSFileSystemTest, CombineDirectory) {
    WBE::Directory expected_absolute({ "hello", "world", "this", "is", "a", "test", "directory" }, true);
    WBE::Directory dir1({ "hello", "world" }, true);
    WBE::Directory dir = dir1.combine(WBE::Directory({ "this", "is", "a", "test", "directory" }, false));
    ASSERT_EQ(expected_absolute, dir);
    WBE::Directory expected_relative({ "hello", "world", "this", "is", "a", "test", "directory" }, false);
    WBE::Directory dir2({ "hello", "world" }, false);
    WBE::Directory dir_rel = dir2.combine(WBE::Directory({ "this", "is", "a", "test", "directory" }, false));
    ASSERT_EQ(expected_relative, dir_rel);
    ASSERT_EQ(dir1.combine(WBE::Directory(std::vector<std::string>())), dir1);
    ASSERT_EQ(dir2.combine(WBE::Directory(std::vector<std::string>())), dir2);
}

#endif
