"""Measure recorder cost using the binaries produced by check.sh."""

import argparse
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile


def measure(binaries: Path) -> dict:
    samples = {name: [] for name in ("compiled_out", "runtime_off", "wall", "wall_cpu")}
    with tempfile.TemporaryDirectory(prefix="nimf-profile-overhead-") as directory:
        for trial in range(5):
            modes = list(samples)
            if trial % 2:
                modes.reverse()
            for mode in modes:
                env = {**os.environ, "NIMF_PROFILE_CAPACITY": "131072",
                       "NIMF_PROFILE_DIR": directory if mode in ("wall", "wall_cpu") else "",
                       "NIMF_PROFILE_CPU": "1" if mode == "wall_cpu" else "0"}
                binary = binaries / ("disabled" if mode == "compiled_out" else "enabled")
                result = subprocess.run([str(binary), "calibrate"], env=env, check=True,
                                        capture_output=True, text=True)
                if result.stderr:
                    raise RuntimeError(result.stderr)
                samples[mode].append(float(result.stdout))
    return {name: {"ns_per_scope": values, "median_ns": statistics.median(values),
                   "min_ns": min(values), "max_ns": max(values)} for name, values in samples.items()}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binaries", type=Path)
    arguments = parser.parse_args()
    print(json.dumps(measure(arguments.binaries.resolve()), indent=2))
