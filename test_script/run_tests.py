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
import os
import sys
import subprocess

class WBETest:
    """Test class. Used for test running.

    Attributes: 
        is_failed: True if any tests has failed. False otherwise.
    """

    def __init__(self):
        self.is_failed = True

    def run_tests(self, cmd = []) -> None:
        """Run tests.

        Args:
            cmd (): The runner cmd to run the test. i.g. valgrind
        """
        failed_count : int = 0
        for test_path in sys.argv[1:]:
            print(f"Running test: {test_path}")
            result = subprocess.run(cmd + [test_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            print(result.stdout)
            if result.returncode != 0:
                print(f"Test {test_path} failed! returned {result.returncode}", file=sys.stderr)
                print(result.stderr, file=sys.stderr)
                failed_count += 1
        if failed_count == 0:
            self.is_failed = False
            print("Test finish! all tests passed.")
        elif failed_count == 1:
            print("Test finish! 1 test failed.")
        else:
            print("Test finish!", failed_count, "tests failed.")

if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print("Usage: " + sys.argv[0] + " [test_path1] [test_path2] ...")
        exit(-1)
    print("White Bird Engine running tests...")

    RUNNER_CMD = []
    if os.name == "posix":
        print("Using Valgrind")
        RUNNER_CMD = [
            'valgrind',
            '--leak-check=full',
            '--show-leak-kinds=all',
            '--error-exitcode=42',
        ]

    os.chdir(os.getcwd() + "/..")
    test_runner = WBETest()
    test_runner.run_tests(RUNNER_CMD)
    print("White Bird Engine tests finished.")
    if (test_runner.is_failed):
        exit(-1)

