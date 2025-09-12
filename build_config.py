target_info = {
    "debug" : {
        "export-directory" : "build",
        "cmake-build-type" : "Debug",
        "generate-tests" : True,
    },
    "release" : {
        "export-directory" : "release",
        "cmake-build-type" : "Release",
        "generate-tests" : True,
    },
    "deploy" : {
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

