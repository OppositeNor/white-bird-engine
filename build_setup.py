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
import shutil
import build_config
from build_script.utils import list_files

# SETUP
print("WBEBuilder: Setting up...")
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
build_dir = os.path.join(root_dir, os.path.join("build", build_target["export-directory"]))
dependencies_dir = os.path.join(root_dir, "dependencies")
template_dir = os.path.join(root_dir, "templates")
# Resource dirs
resource_dir = os.path.join(root_dir, build_config.resource_dir)
resource_output_dir = os.path.join(build_dir, "res")
test_env_resource_dir = os.path.join(test_env_dir, "res")
config_dir = os.path.join(resource_dir, "config")
config_output_dir = os.path.join(resource_output_dir, "config")
shaders_dir = os.path.join(resource_dir, "shaders")
shaders_output_dir = os.path.join(resource_output_dir, "shaders")
res_chunks_dir = os.path.join(resource_dir, "res_chunks")
res_chunks_output_dir = os.path.join(resource_output_dir, "res_chunks")
assets_dir = os.path.join(resource_dir, "assets")
assets_output_dir = os.path.join(resource_output_dir, "assets")
metadata_path = os.path.join(resource_output_dir, "metadata.json")
metadata_cache_dir = os.path.join(build_dir, "metadata_cache")
licenses_output_dir = os.path.join(resource_output_dir, "licenses")

gen_info_files = list_files(root_dir, ignore_dirs=[".cache", ".git",
                                                   ".github", "__pycache__", "build"])
gen_info_files = [gen_info for gen_info in gen_info_files if os.path.basename(gen_info) == "generate.json"]
project_files = list_files(root_dir, ignore_dirs=["dependencies", "generated",
                                                  ".cache", ".git", ".github", "__pycache__",
                                                  "build"])
project_files_exclude_tests = list_files(root_dir, ignore_dirs=["tests", "dependencies", "generated",
                                                                ".cache", ".git", ".github", "__pycache__",
                                                                "build"])

# Create directories
os.makedirs(metadata_cache_dir, exist_ok=True)
os.makedirs(resource_dir, exist_ok=True)
os.makedirs(shaders_output_dir, exist_ok=True)
os.makedirs(licenses_output_dir, exist_ok=True)
shutil.copytree(config_dir, config_output_dir, dirs_exist_ok=True)
shutil.copytree(res_chunks_dir, res_chunks_output_dir, dirs_exist_ok=True)
shutil.copytree(assets_dir, assets_output_dir, dirs_exist_ok=True)

