target_info = {
    "debug" : {
        "c-compiler" : "clang",
        "cpp-compiler" : "clang++",
        "export-directory" : "debug",
        "cmake-build-type" : "Debug",
        "generate-tests" : True,
    },
    "release" : {
        "c-compiler" : "clang",
        "cpp-compiler" : "clang++",
        "export-directory" : "release",
        "cmake-build-type" : "Release",
        "generate-tests" : True,
    },
    "deploy" : {
        "c-compiler" : "clang",
        "cpp-compiler" : "clang++",
        "export-directory" : "deploy",
        "cmake-build-type" : "Release",
        "generate-tests" : False,
    },
    "debug-gcc" : {
        "c-compiler" : "gcc",
        "cpp-compiler" : "g++",
        "export-directory" : "build",
        "cmake-build-type" : "Debug",
        "generate-tests" : True,
    },
    "release-gcc" : {
        "c-compiler" : "gcc",
        "cpp-compiler" : "g++",
        "export-directory" : "release",
        "cmake-build-type" : "Release",
        "generate-tests" : True,
    },
    "deploy-gcc" : {
        "c-compiler" : "gcc",
        "cpp-compiler" : "g++",
        "export-directory" : "deploy",
        "cmake-build-type" : "Release",
        "generate-tests" : False,
    },
}

default_target = "deploy"

source_extensions = set([".cpp", ".hh"])
include_dir = "include"
source_dir = "src"
test_dir = "tests"
resource_dir = "res"

