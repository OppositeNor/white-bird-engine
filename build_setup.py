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
from build_script.project.project import load_project
from build_script.resource.acp.acp import ACP
from build_script.build_dependency.build_dependency import BuildDependencies
from build_script.build_dependency.build_dependency_acp import BuildDependencyACP
from build_script.build_dependency.build_dependency_slangpy import BuildDependencySlangPy

# SETUP
print("WBEBuilder: Setting up...")
# Parse CLA
arg_parser = ArgumentParser(
    prog=sys.argv[0],
    description="Build script to build White Bird Engine."
)
arg_parser.add_argument("-t", "--target", help="Build target.", choices=list(build_config.target_info.keys()), default=build_config.default_target)
arg_parser.add_argument("-p", "--project", help="Directory of a game project (containing wbe_project.toml) to build against the engine.",
                        default=None)
args = arg_parser.parse_args()

# Copy the target so that project builds can adjust it without changing build_config.
build_target = dict(build_config.target_info[args.target])

# Sources dirs
root_dir = build_config.root_dir
include_dir = os.path.join(root_dir, build_config.include_dir)
source_dir = os.path.join(root_dir, build_config.source_dir)
test_dir = os.path.join(root_dir, build_config.test_dir)
test_env_dir = os.path.join(root_dir, build_config.test_env_dir)
# Engine build dependencies (build/dependencies) are shared by the engine and every project built against it.
dependency_build_dir = os.path.join(root_dir, "build", build_config.dependency_build_dir)

# Project. Without --project, the engine repository is the project.
project = load_project(args.project) if args.project is not None else None
if project is not None:
    # Projects build only the engine library and their own targets; the engine's tests are built from the engine repository.
    build_target["generate-tests"] = False
project_dir = project.root_dir if project is not None else root_dir

build_root_dir = os.path.join(project_dir, "build")
build_dir = os.path.join(build_root_dir, build_target["export-directory"])
binary_dir = os.path.join(build_dir, "bin")
# Reflection output, regenerated each build: headers are included as "generated/<name>.gen.hh".
per_target_include_dir = os.path.join(build_dir, "generated", "include")
generated_source_dir = os.path.join(build_dir, "generated", "src")
dependencies_dir = os.path.join(root_dir, "dependencies")
template_dir = os.path.join(root_dir, "templates")
teal_dir = os.path.join(dependencies_dir, "tl")
# Resource dirs
resource_dir = os.path.join(root_dir, build_config.resource_dir)
resource_output_dir = os.path.join(binary_dir, "res")
test_env_res_output_dir = os.path.join(test_env_dir, "res")
config_dir = os.path.join(resource_dir, "config")
config_output_dir = os.path.join(resource_output_dir, "config")
test_env_config_dir = os.path.join(config_dir, "test")
test_env_config_output_dir = os.path.join(test_env_res_output_dir, "config")
shaders_incl_dir = os.path.join(resource_dir, "assets/shaders")
res_chunks_dir = os.path.join(resource_dir, "res_chunks")
res_chunks_output_dir = os.path.join(resource_output_dir, "res_chunks")
assets_dir = os.path.join(resource_dir, "assets")
# Script resources' ids are their module paths under this directory.
script_modules_dir = os.path.join(assets_dir, "scripts", "teal")
# The generated Teal declarations of the reflected engine types (wbe.d.tl).
script_types_dir = os.path.join(build_dir, "teal")
assets_output_dir = os.path.join(resource_output_dir, "assets")
metadata_path = os.path.join(test_env_dir, "metadata.json")
# Project resources are compiled into the same runtime resource directory, after the engine's, and may use the engine's
# shader includes and script modules.
project_resource_dir = project.resource_dir if project is not None else None
project_shaders_incl_dir = os.path.join(project_resource_dir, "assets", "shaders") if project_resource_dir is not None else None
project_script_modules_dir = (os.path.join(project_resource_dir, "assets", "scripts", "teal")
                              if project_resource_dir is not None else script_modules_dir)
cache_dir = os.path.join(build_dir, "build_cache")
metadata_cache_dir = os.path.join(cache_dir, "source_metadata")
model_compiler_cache_dir = os.path.join(cache_dir, "model_compiler")
licenses_output_dir = os.path.join(resource_output_dir, "licenses")

# Create directories
os.makedirs(cache_dir, exist_ok=True)
os.makedirs(metadata_cache_dir, exist_ok=True)
os.makedirs(model_compiler_cache_dir, exist_ok=True)
os.makedirs(resource_dir, exist_ok=True)
os.makedirs(licenses_output_dir, exist_ok=True)
shutil.copytree(config_dir, config_output_dir, dirs_exist_ok=True, ignore=shutil.ignore_patterns("test"))
if project_resource_dir is not None and os.path.isdir(os.path.join(project_resource_dir, "config")):
    # Project configuration files override the engine's files of the same name.
    shutil.copytree(os.path.join(project_resource_dir, "config"), config_output_dir, dirs_exist_ok=True)
if build_target["generate-tests"]:
    shutil.copytree(config_dir, test_env_config_output_dir, dirs_exist_ok=True, ignore=shutil.ignore_patterns("test"))
    shutil.copytree(test_env_config_dir, test_env_config_output_dir, dirs_exist_ok=True)

# Setup ACP
acp_dependencies_compiler = BuildDependencyACP(
    Path(root_dir), Path(dependency_build_dir), Path(dependencies_dir), build_config.dependency_build_info)
acp = ACP(Path(resource_dir), Path(resource_output_dir), enabled_test=False)
test_env_acp = ACP(Path(resource_dir), Path(test_env_res_output_dir), enabled_test=True)
project_acp = (ACP(Path(project_resource_dir), Path(resource_output_dir), enabled_test=False)
               if project_resource_dir is not None else None)
acp_compilers_configured = False

# Setup build dependencies
build_dependencies = BuildDependencies(Path(dependency_build_dir))
build_dependencies.add(acp_dependencies_compiler)
if build_target["generate-tests"]:
    build_dependencies.add(BuildDependencySlangPy(
        Path(dependency_build_dir), Path(dependencies_dir), build_config.dependency_build_info))


def setup_acp_compilers() -> None:
    global acp_compilers_configured
    if acp_compilers_configured:
        return
    python_package_path = acp_dependencies_compiler.get_python_package_dir().resolve().as_posix()
    if python_package_path not in sys.path:
        sys.path.insert(0, python_package_path)

    from build_script.resource.acp.acp_compiler_file import ACPCompilerFile
    from build_script.resource.acp.acp_compiler_mesh import ACPCompilerMesh
    from build_script.resource.acp.acp_compiler_model import ACPCompilerModel
    from build_script.resource.acp.acp_compiler_script import ACPCompilerScript
    from build_script.resource.acp.acp_compiler_shader import ACPCompilerShader
    from build_script.resource.acp.acp_compiler_texture import TextureCompilerPyKTX, ACPCompilerTexture
    from build_script.resource.acp.sdf_compiler_mesh_to_sdf import SDFCompilerMeshToSDF

    texture_compiler = TextureCompilerPyKTX(
        acp_dependencies_compiler.get_ktx_library_dir(), acp_dependencies_compiler.get_python_package_dir())
    sdf_compiler = SDFCompilerMeshToSDF(texture_compiler)
    def add_compilers(p_acp: ACP, p_shader_include_dirs: list[str], p_script_module_dirs: list[str]) -> None:
        p_acp.add_compiler(ACPCompilerShader([Path(include_dir) for include_dir in p_shader_include_dirs]))
        p_acp.add_compiler(ACPCompilerTexture(texture_compiler))
        p_acp.add_compiler(ACPCompilerModel(texture_compiler, sdf_compiler, Path(model_compiler_cache_dir)))
        p_acp.add_compiler(ACPCompilerMesh(sdf_compiler))
        p_acp.add_compiler(ACPCompilerFile(["audio", "binary", "font", "image", "text", "video"]))
        p_acp.add_compiler(ACPCompilerScript(
            Path(teal_dir), [Path(module_dir) for module_dir in p_script_module_dirs], Path(script_types_dir)))

    add_compilers(acp, [shaders_incl_dir], [script_modules_dir, script_types_dir])
    add_compilers(test_env_acp, [shaders_incl_dir], [script_modules_dir, script_types_dir])
    if project_acp is not None and project_shaders_incl_dir is not None:
        # Project modules and shader includes are searched before the engine's.
        add_compilers(project_acp, [project_shaders_incl_dir, shaders_incl_dir],
                      [project_script_modules_dir, script_modules_dir, script_types_dir])
    acp_compilers_configured = True
