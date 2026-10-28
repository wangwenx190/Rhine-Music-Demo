"""Give each CTest invocation an unused output path, retaining prior success and failure evidence."""
from pathlib import Path
import subprocess
import sys
import uuid

executable, base, *arguments = sys.argv[1:]
output = Path(base) / str(uuid.uuid4())
print(f"Verification output: {output}", flush=True)
if any("{output}" in argument for argument in arguments):
    command = [executable, *[argument.replace("{output}", str(output)) for argument in arguments]]
else:
    command = [executable, str(output), *arguments]
raise SystemExit(subprocess.run(command).returncode)
