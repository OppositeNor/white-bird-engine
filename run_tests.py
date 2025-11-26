import os
import subprocess
from build_setup import binary_dir

run_script = ["valgrind", "--error-exitcode=42", str(os.path.join(binary_dir, 'tests/wbe_unit_test'))]

subprocess.run(run_script)
