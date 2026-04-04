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
from pathlib import Path
import sys
import shutil
import build_config
from build_script.resource.acp.acp import WBEACP
from build_script.resource.acp.acp_compiler_shader import WBEACPCompilerShader
from build_script.resource.acp.asp_compiler_discard import WBEACPCompilerDiscard

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

# Sources dirs
root_dir = build_config.root_dir
include_dir = os.path.join(root_dir, build_config.include_dir)
source_dir = os.path.join(root_dir, build_config.source_dir)
test_dir = os.path.join(root_dir, build_config.test_dir)
test_env_dir = os.path.join(root_dir, build_config.test_env_dir)
build_root_dir = os.path.join(root_dir, "build")
build_dir = os.path.join(build_root_dir, build_target["export-directory"])
binary_dir = os.path.join(build_dir, "bin")
dependencies_dir = os.path.join(root_dir, "dependencies")
template_dir = os.path.join(root_dir, "templates")
# Resource dirs
resource_dir = os.path.join(root_dir, build_config.resource_dir)
test_env_res_dir = os.path.join(root_dir, build_config.test_env_res_dir)
resource_output_dir = os.path.join(binary_dir, "res")
test_env_res_output_dir = os.path.join(test_env_dir, "res")
config_dir = os.path.join(resource_dir, "config")
config_output_dir = os.path.join(resource_output_dir, "config")
shaders_incl_dir = os.path.join(resource_dir, "assets/shaders")
test_env_shaders_incl_dir = os.path.join(test_env_res_dir, "assets/shaders")
res_chunks_dir = os.path.join(resource_dir, "res_chunks")
res_chunks_output_dir = os.path.join(resource_output_dir, "res_chunks")
assets_dir = os.path.join(resource_dir, "assets")
assets_output_dir = os.path.join(resource_output_dir, "assets")
metadata_path = os.path.join(test_env_dir, "metadata.json")
metadata_cache_dir = os.path.join(build_dir, "metadata_cache")
licenses_output_dir = os.path.join(resource_output_dir, "licenses")

# Create directories
os.makedirs(metadata_cache_dir, exist_ok=True)
os.makedirs(resource_dir, exist_ok=True)
os.makedirs(licenses_output_dir, exist_ok=True)
shutil.copytree(config_dir, config_output_dir, dirs_exist_ok=True)

# Setup ACP
acp = WBEACP(Path(resource_dir), Path(resource_output_dir))
acp.add_compiler(WBEACPCompilerShader(Path(shaders_incl_dir)))
acp.add_compiler(WBEACPCompilerDiscard([".slangh", ".hlslh"]))

test_env_acp = WBEACP(Path(test_env_res_dir), Path(test_env_res_output_dir))
test_env_acp.add_compiler(WBEACPCompilerShader(Path(test_env_shaders_incl_dir)))
test_env_acp.add_compiler(WBEACPCompilerDiscard([".slangh", ".hlslh"]))

