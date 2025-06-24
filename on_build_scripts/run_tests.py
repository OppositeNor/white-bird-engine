import os
import sys
import subprocess

class WBETest:
    def run_tests(self) -> None:
        failed_count : int = 0
        for test_path in sys.argv[1:]:
            print(f"Running test: {test_path}")
            result = subprocess.run([test_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            print(result.stdout)
            if result.returncode != 0:
                print(f"Test {test_path} failed! returned {result.returncode}", file=sys.stderr)
                print(result.stderr, file=sys.stderr)
                failed_count += 1
        if failed_count == 0:
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
    os.chdir(os.getcwd() + "/..")
    test_runner = WBETest()
    test_runner.run_tests()
    print("White Bird Engine tests finished.")

