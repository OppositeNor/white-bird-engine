# Copyright 2025 OppositeNor
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
target_info = {
    "debug" : {
        "c-compiler" : "clang",
        "cpp-compiler" : "clang++",
        "export-directory" : "build",
        "cmake-build-type" : "Debug",
        "generate-tests" : True,
    },
    "release" : {
        "c-compiler" : "clang",
        "cpp-compiler" : "clang++",
        "export-directory" : "release",
        "cmake-build-type" : "Release",
        "generator" : "Ninja",
        "generate-tests" : True,
    },
    "deploy" : {
        "c-compiler" : "clang",
        "cpp-compiler" : "clang++",
        "export-directory" : "deploy",
        "cmake-build-type" : "Deploy",
        "generator" : "Ninja",
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
        "generator" : "Ninja",
        "generate-tests" : True,
    },
    "deploy-gcc" : {
        "c-compiler" : "gcc",
        "cpp-compiler" : "g++",
        "export-directory" : "deploy",
        "cmake-build-type" : "Deploy",
        "generator" : "Ninja",
        "generate-tests" : False,
    },
}

default_target = "deploy"

source_extensions = set([".cpp", ".hh"])
include_dir = "include"
source_dir = "src"
test_dir = "tests"
test_env_dir = "test_env"
resource_dir = "res"

gen_info = {
    "static_labels" : [
        "WBE_CHANNEL",
        "WBE_RENDER_OBJECT_TYPE",
        "WBE_RENDER_TASK",
    ],
    "dynamic_labels" : [
        "WBE_CHANNEL",
        "WBE_RENDER_OBJECT_TYPE",
        "WBE_RENDER_TASK",
    ],
    "serializables" : [
        "WBE_SERIALIZABLE",
        "WBE_COMPONENT",
        "WBE_CONFIG_OPTION"
    ]
}
