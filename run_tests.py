import os
import subprocess
from build_setup import build_dir

run_script = ["valgrind", str(os.path.join(build_dir, 'tests/wbe_unit_test'))]

subprocess.run(run_script)
