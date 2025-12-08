import os
import subprocess
from build_setup import binary_dir

run_command = ['valgrind',
              '--leak-check=full',
              '--show-leak-kinds=all',
              '--error-exitcode=42',
              str(os.path.join(binary_dir, 'wbe_unit_test'))]

exit(subprocess.run(run_command).returncode)
