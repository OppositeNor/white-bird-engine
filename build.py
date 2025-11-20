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
import json
import os
from pathlib import Path
import subprocess
import traceback
from build_script import on_build
import shutil
import build_config
from build_script.reflection.code_gen import WBEGenFileInfo
import build_setup

metaparser_clang_args = [
    "-std=c++20",
    "-I./include",
    "-DWBE_REFLECTION_PARSER"
]

# HELPER FUNCTIONS
def _get_cmake_command_from_info(build_info):
    """Helper function which converts the build info to cmake command"""
    result = ["cmake"]
    result.append("-B")
    result.append(build_setup.build_dir)
    if build_info.get("generator") is not None:
        result.append("-G")
        result.append(build_info["generator"])
    result.append(f"-DCMAKE_BUILD_TYPE={build_info["cmake-build-type"]}")
    if build_info.get("cpp-compiler") is not None:
        result.append(f"-DCMAKE_CXX_COMPILER={build_info["cpp-compiler"]}")
    if build_info.get("c-compiler") is not None:
        result.append(f"-DCMAKE_C_COMPILER={build_info["c-compiler"]}")
    if build_info["generate-tests"]:
        result.append("-DWBE_MAKE_TEST=ON")
    else:
        result.append("-DWBE_MAKE_TEST=OFF")
    if build_info.get("additional-cmake-args") is not None:
        result.append(build_info["additional-cmake-args"])
    return result

def _gather_license():
    # Get white bird engine license
    shutil.copyfile(os.path.join(build_setup.root_dir, "LICENSE"), os.path.join(build_setup.licenses_output_dir, "white-bird-engine_LICENSE"))
    shutil.copyfile(os.path.join(build_setup.root_dir, "NOTICE"), os.path.join(build_setup.licenses_output_dir, "white-bird-engine_NOTICE"))
    # Get licenses from dependencies
    deps_dir = Path(build_setup.dependencies_dir).resolve()
    output_dir = Path(build_setup.licenses_output_dir).resolve()

    for subdir in deps_dir.iterdir():
        if subdir.is_dir():
            license_files = [p for p in subdir.iterdir() 
                             if p.is_file() and ("license" in p.name.lower() or
                                                 "copying" in p.name.lower() or
                                                 "authors" in p.name.lower() or
                                                 "patents" in p.name.lower())]
            if not license_files:
                print(f"Warning: No LICENSE found in {subdir}")
                continue
            for lf in license_files:
                target_name = f"{subdir.name}_{lf.name}"
                target_path = os.path.join(output_dir, target_name)
                shutil.copy2(lf, target_path)

            # Apache license requires to also include a NOTICE file if exists:
            notice_files = [p for p in subdir.iterdir()
                if p.is_file() and "notice" in p.name.lower()]
            if notice_files:
                for nf in notice_files:
                    target_name = f"{subdir.name}_{nf.name}"
                    target_path = os.path.join(output_dir, target_name)
                    print(f"Copying NOTICE {nf} -> {target_path}")
                    shutil.copy2(nf, target_path)

def _compile_shaders():
    shaders_dir_path = Path(build_setup.shaders_dir)
    for hlsl_file in shaders_dir_path.rglob("*.hlsl"):
        with open(hlsl_file, "r", encoding="utf-8") as f:
            first_line = f.readline().strip()
        if first_line.startswith("//"):
            shader_stage = first_line[2:].strip()
            if shader_stage not in ["vertex", "vert", "fragment", "frag", "tesscontrol",
                    "tesc", "tesseval", "tese", "geometry", "geom", "compute", "comp"]:
                print(f"Unsupported shader stage for {hlsl_file} : {shader_stage}, skipped.")
                continue
        else:
            # Files without a profile header are considered to be header files, skipped.
            continue

        output_file = os.path.join(build_setup.shaders_output_dir, (hlsl_file.stem + ".spv"))
        cmd = [
            "glslc",
            f"-fshader-stage={shader_stage}",
            "-I", build_setup.shaders_dir,
            "-fentry-point=main",
            "-o", str(output_file),
            str(hlsl_file)
        ]
        print("Compiling:", hlsl_file)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Failed to compile", hlsl_file)
            print(result.stderr)
            raise RuntimeError("Failed to compile shader.")

def _gather_gen_infos():
    gen_infos = []
    for gen_info_file in build_setup.gen_info_files:
        with open(gen_info_file) as f:
            data = json.load(f)
        file_infos = [WBEGenFileInfo(**info) for info in data]
        for file_info in file_infos:
            file_info.out_dir = os.path.dirname(gen_info_file)
        gen_infos.extend(file_infos)
    return gen_infos

# ENTRY
if __name__ == "__main__":
    # Gather sources for reflection
    try:
        print(f"WBEBuilder: Building target: {build_setup.args.target}.")
        print("WBEBuilder: Compiling shaders...")
        _compile_shaders()
        print("WBEBuilder: Gathering licenses...")
        _gather_license()
        print("WBEBuilder: Gathering sources...")
        if build_setup.build_target["generate-tests"]:
            sources = [source for source in build_setup.project_files if Path(source).suffix in build_config.source_extensions]
        else:
            sources = [source for source in build_setup.project_files_exclude_tests if Path(source).suffix in build_config.source_extensions]
        print("WBEBuilder: Gathering generate.json...")
        gen_infos = _gather_gen_infos()

        # Run reflection script
        print("WBEBuilder: Running reflections...")
        on_build.reflect(metaparser_clang_args, build_setup.metadata_path, build_setup.metadata_cache_dir, sources, gen_infos)

        # Build project with CMake
        print("WBEBuilder: Running cmake...")
        result = subprocess.run(_get_cmake_command_from_info(build_setup.build_target))
        if result.returncode != 0:
            raise RuntimeError("Failed to setup cmake build.")
        build_command = ["cmake", "--build", build_setup.build_dir]
        if os.cpu_count() is not None:
            build_command.extend(["-j", str(os.cpu_count())])
        result = subprocess.run(build_command)
        if result.returncode != 0:
            raise RuntimeError("Failed to build with CMake.")

        # Set up test environment
        if build_setup.build_target["generate-tests"]:
            print("WBEBuilder: Setting up test environemnt...")
            shutil.copy(os.path.join(build_setup.resource_output_dir, "metadata.json"), os.path.join(build_setup.test_env_resource_dir, "metadata.json"))

        # For some reason my LSP only looks into build directory for compile_commands.json.
        shutil.copy(os.path.join(build_setup.build_dir, "compile_commands.json"), "build")
        print("WBEBuilder: Finished!")

    except Exception as e:
        print("Build failed! Message:", e)
        traceback.print_exc()
        exit(-1)
