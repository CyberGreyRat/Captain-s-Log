from __future__ import annotations

import os
import subprocess
import sys
from importlib import resources
from pathlib import Path


def main() -> int:
    """Forward all CLI arguments to the packaged native CapCom executable."""
    if os.name != "nt":
        print(
            "CapCom CLI currently contains a Windows executable and can only "
            "run on Windows.",
            file=sys.stderr,
        )
        return 3

    executable_resource = resources.files("capcom").joinpath("bin", "cap.exe")

    try:
        # Keep the resource context active for the complete child process.
        with resources.as_file(executable_resource) as executable_path:
            executable = Path(executable_path)
            if not executable.is_file():
                print(
                    "CapCom distribution error: packaged executable missing "
                    "(capcom/bin/cap.exe).",
                    file=sys.stderr,
                )
                return 4

            completed = subprocess.run(
                [str(executable), *sys.argv[1:]],
                check=False,
                cwd=Path.cwd(),
            )
            return completed.returncode
    except OSError as error:
        print(f"Could not start CapCom CLI: {error}", file=sys.stderr)
        return 5


if __name__ == "__main__":
    raise SystemExit(main())
