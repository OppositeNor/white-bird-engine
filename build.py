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
from argparse import ArgumentParser
import os
import sys
import subprocess
from build_script import on_build
from build_script import utils
import shutil
import build_config

metaparser_clang_args = [
    "-std=c++20",
    "-I./include",
    "-DWBE_REFLECTION_PARSER"
]

print("WBEBuilder: Setting up...")

# SETUP
# Parse CLA
arg_parser = ArgumentParser(
    prog=sys.argv[0],
    description="Build script to build White Bird Engine."
)
arg_parser.add_argument("-t", "--target", help="Build target.", choices=list(build_config.target_info.keys()), default=build_config.default_target)
args = arg_parser.parse_args()

build_target = build_config.target_info[args.target]

# Setup directories
root_dir = os.path.abspath(os.path.dirname(os.path.abspath(__file__)))
# Sources dirs
include_dir = os.path.join(root_dir, build_config.include_dir)
source_dir = os.path.join(root_dir, build_config.source_dir)
test_dir = os.path.join(root_dir, build_config.test_dir)
test_env_dir = os.path.join(root_dir, build_config.test_env_dir)
build_dir = os.path.join(root_dir, build_target["export-directory"])
# Resource dirs
resource_dir = os.path.join(root_dir, build_config.resource_dir)
resource_output_dir = os.path.join(build_dir, "res")
test_env_resource_dir = os.path.join(test_env_dir, "res")
config_dir = os.path.join(resource_dir, "config")
config_output_dir = os.path.join(resource_output_dir, "config")
shaders_dir = os.path.join(resource_dir, "shaders")
shaders_output_dir = os.path.join(resource_output_dir, "shaders")
metadata_path = os.path.join(resource_output_dir, "metadata.json")
metadata_cache_dir = os.path.join(build_dir, "metadata_cache")

# Create directories
os.makedirs(metadata_cache_dir, exist_ok=True)
os.makedirs(resource_dir, exist_ok=True)
os.makedirs(shaders_output_dir, exist_ok=True)
shutil.copytree(config_dir, config_output_dir, dirs_exist_ok=True)

# HELPER FUNCTIONS
def _get_cmake_command_from_info(p_build_info):
    """Helper function which converts the build info to cmake command"""
    result = ["cmake"]
    result.append("-B")
    result.append(p_build_info["export-directory"])
    if p_build_info.get("generator") is not None:
        result.append("-G")
        result.append(p_build_info["generator"])
    result.append(f"-DCMAKE_BUILD_TYPE={p_build_info["cmake-build-type"]}")
    if p_build_info.get("cpp-compiler") is not None:
        result.append(f"-DCMAKE_CXX_COMPILER={p_build_info["cpp-compiler"]}")
    if p_build_info.get("c-compiler") is not None:
        result.append(f"-DCMAKE_C_COMPILER={p_build_info["c-compiler"]}")
    if p_build_info["generate-tests"]:
        result.append("-DWBE_MAKE_TEST=ON")
    else:
        result.append("-DWBE_MAKE_TEST=OFF")
    if p_build_info.get("additional-cmake-args") is not None:
        result.append(p_build_info["additional-cmake-args"])
    return result

# ENTRY
# Gather sources for reflection
try:
    print(f"WBEBuilder: Building target: {args.target}.")
    print("WBEBuilder: Gathering sources...")
    sources = []
    sources.extend(utils.gather_files(include_dir, build_config.source_extensions))
    sources.extend(utils.gather_files(source_dir, build_config.source_extensions))
    if build_target["generate-tests"]:
        sources.extend(utils.gather_files(test_dir, build_config.source_extensions))

    # Run reflection script
    print("WBEBuilder: Running reflections...")
    on_build.reflect(metaparser_clang_args, metadata_path, metadata_cache_dir, sources)

    # Build project with CMake
    print("WBEBuilder: Running cmake...")
    result = subprocess.run(_get_cmake_command_from_info(build_target))
    if result.returncode != 0:
        raise RuntimeError("Failed to setup cmake build.")
    result = subprocess.run(["cmake", "--build", build_target["export-directory"]])
    if result.returncode != 0:
        raise RuntimeError("Failed to build with CMake.")

    # Set up test environment
    if build_target["generate-tests"]:
        print("WBEBuilder: Setting up test environemnt...")
        shutil.copy(os.path.join(resource_output_dir, "metadata.json"), os.path.join(test_env_resource_dir, "metadata.json"))

    print("WBEBuilder: Finished!")

except Exception as e:
    print("Build failed! Message:", e)
    exit(-1)

