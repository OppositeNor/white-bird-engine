/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_FILE_SYSTEM_TEST_HH__
#define __WBE_FILE_SYSTEM_TEST_HH__

#include "platform/file_system/directory.hh"
#include "platform/file_system/file_system.hh"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(LinuxFileSystemTest, DirEquals) {
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

TEST(LinuxFileSystemTest, DirToString) {
    WBE::Directory test_dir({ "hello", "world", "this", "is", "a", "test", "directory" }, false);
    ASSERT_EQ(WBE::FileSystem::dir_to_string(test_dir), std::string("hello/world/this/is/a/test/directory/"));
    ASSERT_EQ(std::string(test_dir), std::string("hello/world/this/is/a/test/directory/"));
    WBE::Directory test_dir_absolute({ "this", "is", "a", "absolute", "test", "directory" }, true);
    ASSERT_EQ(WBE::FileSystem::dir_to_string(test_dir_absolute), std::string("/this/is/a/absolute/test/directory/"));
    ASSERT_EQ(std::string(test_dir_absolute), std::string("/this/is/a/absolute/test/directory/"));
}

TEST(LinuxFileSystemTest, ParseDirectory) {
    WBE::Directory expected({ "hello", "world", "this", "is", "a", "test", "directory" }, false);
    WBE::Directory parsed_dir = WBE::FileSystem::parse_directory("hello/world//this/is/not/../a/test/./directory");
    ASSERT_EQ(parsed_dir, expected);
    WBE::Directory parsed_dir_1 = WBE::FileSystem::parse_directory("../hello/world/this/is/not/../a/test/./directory/XD/../");
    ASSERT_EQ(parsed_dir_1, expected);
    WBE::Directory parsed_dir_2 = WBE::FileSystem::parse_directory("./hello/world/this/is/not/../a/test/./directory/XD/../");
    ASSERT_EQ(parsed_dir_2, expected);
    WBE::Directory expected_absolute({ "hello", "this", "is", "me" }, true);
    WBE::Directory parsed_dir_absolute = WBE::FileSystem::parse_directory("/../hello/this/is/me/");
    ASSERT_EQ(parsed_dir_absolute, expected_absolute);
    WBE::Directory parsed_dir_absolute_1 = WBE::FileSystem::parse_directory("/./../hello/that/../this/is/me/");
    ASSERT_EQ(parsed_dir_absolute_1, expected_absolute);
    WBE::Directory parsed_dir_absolute_2 = WBE::FileSystem::parse_directory("/../hello/this/is/me/");
    ASSERT_EQ(parsed_dir_absolute_2, expected_absolute);
    WBE::Directory expectd_relative_empty({}, false);
    WBE::Directory expectd_absolute_empty({}, true);
    WBE::Directory parsed_dir_relative_empty = WBE::FileSystem::parse_directory("");
    WBE::Directory parsed_dir_absolute_empty = WBE::FileSystem::parse_directory("/");
    ASSERT_EQ(parsed_dir_relative_empty, expectd_relative_empty);
    ASSERT_EQ(parsed_dir_absolute_empty, expectd_absolute_empty);
}

TEST(LinuxFileSystemTest, ParseDirectoryEmptyEdgeCases) {
    // Test specific edge case where splitting results in empty array
    WBE::Directory expected_empty({}, false);
    
    // Empty string should return empty relative directory
    WBE::Directory parsed_empty = WBE::FileSystem::parse_directory("");
    ASSERT_EQ(parsed_empty, expected_empty);
    
    // String with only separators and dots should result in appropriate directories
    WBE::Directory parsed_dots_only = WBE::FileSystem::parse_directory("./././.");
    ASSERT_EQ(parsed_dots_only, expected_empty);
    
    // String with only double dots that cancel out
    WBE::Directory parsed_cancelled = WBE::FileSystem::parse_directory("../dir/../");
    ASSERT_EQ(parsed_cancelled, expected_empty);
    
    // Multiple slashes should be treated as single separators
    WBE::Directory parsed_multiple_slashes = WBE::FileSystem::parse_directory("///");
    WBE::Directory expected_absolute_empty({}, true);
    ASSERT_EQ(parsed_multiple_slashes, expected_absolute_empty);
    
    // Complex case that results in empty after processing
    WBE::Directory parsed_complex_empty = WBE::FileSystem::parse_directory("./a/../b/../c/../");
    ASSERT_EQ(parsed_complex_empty, expected_empty);
    
    // Verify splitted.size() == 0 condition is properly handled
    WBE::Directory parsed_only_separators = WBE::FileSystem::parse_directory("////");
    ASSERT_EQ(parsed_only_separators, expected_absolute_empty);
}

TEST(LinuxFileSystemTest, CombineDirectory) {
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

TEST(LinuxFileSystemTest, GetFileName) {
    ASSERT_EQ(WBE::FileSystem::get_file_name("hello/world/this/is/a/test/file.txt"), std::string("file.txt"));
    ASSERT_EQ(WBE::FileSystem::get_file_name("/hello/world/this/is/a/test/file.txt"), std::string("file.txt"));
    ASSERT_EQ(WBE::FileSystem::get_file_name("file.txt"), std::string("file.txt"));
    ASSERT_EQ(WBE::FileSystem::get_file_name("/file.txt"), std::string("file.txt"));
    ASSERT_EQ(WBE::FileSystem::get_file_name("hello/world/this/is/a/test/directory/"), std::string(""));
    ASSERT_EQ(WBE::FileSystem::get_file_name("/hello/world/this/is/a/test/directory/"), std::string(""));
    ASSERT_EQ(WBE::FileSystem::get_file_name(""), std::string(""));
    ASSERT_EQ(WBE::FileSystem::get_file_name("/"), std::string(""));
    ASSERT_EQ(WBE::FileSystem::get_file_name("/////"), std::string(""));
    ASSERT_EQ(WBE::FileSystem::get_file_name("/////file.txt"), std::string("file.txt"));
}

TEST(LinuxFileSystemTest, GetFileDir) {
    WBE::Directory expected1({ "hello", "world", "this", "is", "a", "test" }, false);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("hello/world/this/is/a/test/file.txt"), expected1);
    WBE::Directory expected2({ "hello", "world", "this", "is", "a", "test" }, true);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("/hello/world/this/is/a/test/file.txt"), expected2);
    WBE::Directory expected3({}, false);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("file.txt"), expected3);
    WBE::Directory expected4({}, true);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("/file.txt"), expected4);
    WBE::Directory expected5({ "hello", "world", "this", "is", "a", "test", "directory" }, false);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("hello/world/this/is/a/test/directory/"), expected5);
    WBE::Directory expected6({ "hello", "world", "this", "is", "a", "test", "directory" }, true);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("/hello/world/this/is/a/test/directory/"), expected6);
    WBE::Directory expected5_alt({ "hello", "world", "this", "is", "a", "test" }, false);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("hello/world/this/is/a/test/directory"), expected5_alt);
    WBE::Directory expected6_alt({ "hello", "world", "this", "is", "a", "test" }, true);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("/hello/world/this/is/a/test/directory"), expected6_alt);
    WBE::Directory expected7({}, false);
    ASSERT_EQ(WBE::FileSystem::get_file_dir(""), expected7);
    WBE::Directory expected8({}, true);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("/"), expected8);
    WBE::Directory expected9({}, true);
    ASSERT_EQ(WBE::FileSystem::get_file_dir("/////"), expected9);
}

TEST(LinuxFileSystemTest, PathToString) {
    WBE::Path path1(WBE::Directory({ "hello", "world", "this", "is", "a", "test" }, false), "file.txt");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path1), std::string("hello/world/this/is/a/test/file.txt"));
    WBE::Path path2(WBE::Directory({ "hello", "world", "this", "is", "a", "test" }, true), "file.txt");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path2), std::string("/hello/world/this/is/a/test/file.txt"));
    WBE::Path path3(WBE::Directory({}, false), "file.txt");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path3), std::string("file.txt"));
    WBE::Path path4(WBE::Directory({}, true), "file.txt");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path4), std::string("/file.txt"));
    WBE::Path path5(WBE::Directory({ "hello", "world", "this", "is", "a", "test", "directory" }, false), "");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path5), std::string("hello/world/this/is/a/test/directory/"));
    WBE::Path path6(WBE::Directory({ "hello", "world", "this", "is", "a", "test", "directory" }, true), "");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path6), std::string("/hello/world/this/is/a/test/directory/"));
    WBE::Path path7(WBE::Directory({}, false), "");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path7), std::string(""));
    WBE::Path path8(WBE::Directory({}, true), "");
    ASSERT_EQ(WBE::FileSystem::path_to_string(path8), std::string("/"));
}

TEST(LinuxFileSystemTest, GetExt) {
    WBE::Path path1(WBE::Directory({ "hello", "world" }, false), "file.txt");
    ASSERT_EQ(WBE::FileSystem::get_ext(path1), std::string(".txt"));
    WBE::Path path2(WBE::Directory({ "hello", "world" }, true), "archive.tar.gz");
    ASSERT_EQ(WBE::FileSystem::get_ext(path2), std::string(".gz"));
    WBE::Path path3(WBE::Directory({}, false), "no_extension");
    ASSERT_EQ(WBE::FileSystem::get_ext(path3), std::string(""));
    WBE::Path path4(WBE::Directory({}, true), ".hiddenfile");
    ASSERT_EQ(WBE::FileSystem::get_ext(path4), std::string(""));
    WBE::Path path5(WBE::Directory({ "some", "path" }, false), "complex.name.with.many.dots.ext");
    ASSERT_EQ(WBE::FileSystem::get_ext(path5), std::string(".ext"));
}

#endif
