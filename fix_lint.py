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

from __future__ import annotations

from argparse import ArgumentParser
from concurrent.futures import ThreadPoolExecutor, as_completed
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import build_config
from build_script.utils import list_files
from git import Repo
from git.exc import BadName, InvalidGitRepositoryError, NoSuchPathError


def find_compile_commands_dir(root_dir: Path) -> Path | None:
    candidates = [
        root_dir,
        root_dir / "build",
        root_dir / "build" / "debug",
        root_dir / "build" / "release",
        root_dir / "build" / "deploy",
        root_dir / "build-debug",
        root_dir / "build-release",
        root_dir / "build-deploy",
    ]

    for candidate in candidates:
        if (candidate / "compile_commands.json").exists():
            return candidate

    for compile_commands in sorted((root_dir / "build").glob("**/compile_commands.json")):
        return compile_commands.parent

    return None


def gather_project_sources(root_dir: Path, include_tests: bool) -> list[str]:
    ignore_dirs = [
        "dependencies",
        "generated",
        ".cache",
        ".git",
        ".github",
        "__pycache__",
        "build",
        "build-debug",
        "build-release",
        "build-deploy",
    ]
    if not include_tests:
        ignore_dirs.append("tests")

    files = list_files(str(root_dir), ignore_dirs=ignore_dirs)
    return [path for path in files if Path(path).suffix in build_config.source_extensions]


def gather_changed_project_sources(root_dir: Path, project_sources: list[str]) -> list[str]:
    try:
        repo = Repo(root_dir, search_parent_directories=True)
    except (InvalidGitRepositoryError, NoSuchPathError):
        print("WBEFixLint: Current workspace is not in a Git repository.")
        return []

    if repo.working_tree_dir is None:
        print("WBEFixLint: Failed to determine Git working tree directory.")
        return []

    repo_root = Path(repo.working_tree_dir).resolve()
    source_set = {str(Path(source).resolve()) for source in project_sources}

    changed_rel_paths: set[str] = set()

    # Unstaged changes.
    for diff in repo.index.diff(None):
        if diff.a_path is not None:
            changed_rel_paths.add(diff.a_path)
        if diff.b_path is not None:
            changed_rel_paths.add(diff.b_path)

    # Staged changes.
    try:
        staged_diffs = repo.index.diff("HEAD")
    except BadName:
        staged_diffs = []
    for diff in staged_diffs:
        if diff.a_path is not None:
            changed_rel_paths.add(diff.a_path)
        if diff.b_path is not None:
            changed_rel_paths.add(diff.b_path)

    # Untracked files.
    for rel_path in repo.untracked_files:
        changed_rel_paths.add(rel_path)

    changed_sources: set[str] = set()
    for rel_path in changed_rel_paths:
        abs_path = str((repo_root / rel_path).resolve())
        if abs_path in source_set:
            changed_sources.add(abs_path)

    return sorted(changed_sources)


def run_clang_tidy_fix(file_path: str, compile_commands_dir: Path) -> subprocess.CompletedProcess[str]:
    cmd = [
        "clang-tidy",
        "--fix",
        "--quiet",
        "-p",
        str(compile_commands_dir),
        file_path,
    ]
    return subprocess.run(cmd, capture_output=True, text=True, check=False)


def has_remaining_lints(file_path: str, compile_commands_dir: Path) -> bool:
    cmd = [
        "clang-tidy",
        "--quiet",
        "-p",
        str(compile_commands_dir),
        file_path,
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    output = f"{result.stdout}\n{result.stderr}"
    return re.search(r"\b(warning|error):", output) is not None


def fix_and_check_file(file_path: str, compile_commands_dir: Path) -> tuple[str, bool, bool]:
    fix_result = run_clang_tidy_fix(file_path, compile_commands_dir)
    has_unfixed_lints = has_remaining_lints(file_path, compile_commands_dir)
    return file_path, fix_result.returncode != 0, has_unfixed_lints


def main() -> int:
    parser = ArgumentParser(
        prog="fix_lint.py",
        description="Run clang-tidy with auto-fix (changed files by default).",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Run lint fix for all project source files instead of only changed files.",
    )
    parser.add_argument(
        "--no-tests",
        action="store_true",
        help="Exclude files under tests/ from lint fixing.",
    )
    parser.add_argument(
        "--compile-commands-dir",
        type=str,
        default=None,
        help="Directory containing compile_commands.json. Auto-detected by default.",
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=max(1, os.cpu_count() or 1),
        help="Number of parallel workers used to run clang-tidy.",
    )
    args = parser.parse_args()

    root_dir = Path(__file__).resolve().parent

    if shutil.which("clang-tidy") is None:
        print("WBEFixLint: clang-tidy is not available in PATH.")
        return 1

    if args.compile_commands_dir is not None:
        compile_commands_dir = Path(args.compile_commands_dir).resolve()
    else:
        found_dir = find_compile_commands_dir(root_dir)
        if found_dir is None:
            print("WBEFixLint: Failed to locate compile_commands.json.")
            print("WBEFixLint: Build the project first or provide --compile-commands-dir.")
            return 1
        compile_commands_dir = found_dir

    if not (compile_commands_dir / "compile_commands.json").exists():
        print(f"WBEFixLint: compile_commands.json not found in {compile_commands_dir}.")
        return 1

    project_sources = gather_project_sources(root_dir, include_tests=not args.no_tests)
    if args.all:
        sources = sorted(project_sources)
    else:
        sources = gather_changed_project_sources(root_dir, project_sources)

    if len(sources) == 0:
        if args.all:
            print("WBEFixLint: No source files found.")
        else:
            print("WBEFixLint: No changed source files found.")
        return 0

    print(f"WBEFixLint: Using compile_commands from {compile_commands_dir}")
    if args.all:
        print("WBEFixLint: Running in full-project mode (--all).")
    else:
        print("WBEFixLint: Running in changed-files mode (default).")
    print(f"WBEFixLint: Lint-fixing {len(sources)} files with {args.jobs} workers...")

    failed_runs: list[str] = []
    unfixed_files: list[str] = []

    total_sources = len(sources)
    completed_count = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as executor:
        future_to_source = {
            executor.submit(fix_and_check_file, source, compile_commands_dir): source
            for source in sources
        }

        for future in as_completed(future_to_source):
            completed_count += 1
            source = future_to_source[future]
            print(f"[{completed_count}/{total_sources}] clang-tidy --fix {source}")
            file_path, fix_failed, has_unfixed_lints = future.result()
            if fix_failed:
                failed_runs.append(file_path)
            if has_unfixed_lints:
                unfixed_files.append(file_path)

    print("WBEFixLint: Finished running clang-tidy.")

    if failed_runs:
        print("WBEFixLint: Warning - clang-tidy failed on these files:")
        for file_path in failed_runs:
            print(f"  - {file_path}")

    if unfixed_files:
        print("WBEFixLint: Warning - these files still have diagnostics after auto-fix:")
        for file_path in unfixed_files:
            print(f"  - {file_path}")

    if failed_runs or unfixed_files:
        return 1

    print("WBEFixLint: No remaining diagnostics were found after auto-fix.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
