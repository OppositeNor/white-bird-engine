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
from pathlib import Path
import build_config
from build_setup import project_files


LICENSE_KEYWORDS = [
    "Licensed under the Apache License, Version 2.0",
    "http://www.apache.org/licenses/LICENSE-2.0"
]

def has_apache_license_header(file_pathes, max_lines=30):
    """Check if all files has an Apache 2.0 license header.

    Args:
        file_pathes (): The pathes of the files to check.
        max_lines (): The maximum lines to check.

    Returns:
        The maximum lines to check.
    """
    failed_checks = []
    for file_path in file_pathes:
        print(f"Checking {file_path}...")
        found = set()
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            for i, line in enumerate(f):
                for k in LICENSE_KEYWORDS:
                    if k in line:
                        found.add(k)
                if max_lines is not None and i + 1 >= max_lines:
                    break
        if not all(k in found for k in LICENSE_KEYWORDS):
            print(f"File {file_path} does not have a license header!")
            failed_checks.append(file_path)

    if len(failed_checks) != 0:
        print(f"{len(file_pathes)} files checked. Files missing a license header:")
        for path in failed_checks:
            print(path)
    else:
        print(f"{len(file_pathes)} files checked. All files contains a license header.")

if __name__ == "__main__":
    sources = [source for source in project_files if Path(source).suffix in build_config.source_extensions]
    has_apache_license_header(sources)
