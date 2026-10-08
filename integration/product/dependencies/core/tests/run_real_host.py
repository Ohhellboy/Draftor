#!/usr/bin/env python3
"""Two actual processes, one fresh isolated state directory, no source mutation."""
from pathlib import Path
import subprocess
import sys
import tempfile

host, first, second, root = sys.argv[1:]
Path(root).mkdir(parents=True, exist_ok=True)
state = tempfile.mkdtemp(prefix="trial-", dir=root)
subprocess.run([host, "write", first, state], check=True)
subprocess.run([host, "read", second, state], check=True)
