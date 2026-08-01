python
import os
import sys

repo_gdb_dir = os.path.join(os.getcwd(), "gdb")
if repo_gdb_dir not in sys.path:
    sys.path.insert(0, repo_gdb_dir)

from memory_pointer import register_memory_pointer_printers
register_memory_pointer_printers(gdb)
end