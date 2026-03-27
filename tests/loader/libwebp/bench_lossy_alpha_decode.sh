#!/bin/sh
# Benchmark static lossy+alpha WebP decode path parity.
# Usage:
#   ./tests/loader/libwebp/bench_lossy_alpha_decode.sh [input.webp] [runs]

set -eu

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
IMG2SIXEL_BIN="${IMG2SIXEL_BIN:-${ROOT_DIR}/converters/img2sixel}"
INPUT_WEBP="${1:-${ROOT_DIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp}"
RUNS="${2:-200}"

if [ ! -x "${IMG2SIXEL_BIN}" ]; then
    echo "error: img2sixel not found: ${IMG2SIXEL_BIN}" >&2
    echo "hint: build first (e.g. make -j4) or set IMG2SIXEL_BIN" >&2
    exit 1
fi

if [ ! -f "${INPUT_WEBP}" ]; then
    echo "error: input file not found: ${INPUT_WEBP}" >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 is required for timing summary" >&2
    exit 1
fi

ROOT_DIR="${ROOT_DIR}" \
IMG2SIXEL_BIN="${IMG2SIXEL_BIN}" \
INPUT_WEBP="${INPUT_WEBP}" \
RUNS="${RUNS}" \
python3 - <<'PY'
import os
import random
import statistics
import subprocess
import time

root = os.environ["ROOT_DIR"]
img2sixel = os.environ["IMG2SIXEL_BIN"]
input_webp = os.environ["INPUT_WEBP"]
runs = int(os.environ["RUNS"])

base_cmd = [img2sixel, "-L", "libwebp!", "-S", input_webp]

def run_one(force_rgb):
    env = os.environ.copy()
    if force_rgb:
        env["SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE"] = "1"
    else:
        env.pop("SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE", None)
    t0 = time.perf_counter()
    subprocess.run(
        base_cmd,
        cwd=root,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=True,
    )
    return (time.perf_counter() - t0) * 1000.0

for _ in range(8):
    run_one(False)
    run_one(True)

labels = ["default"] * runs + ["forced_rgb"] * runs
random.seed(42)
random.shuffle(labels)

default_times = []
forced_times = []
for label in labels:
    if label == "default":
        default_times.append(run_one(False))
    else:
        forced_times.append(run_one(True))

def summary(name, xs):
    ys = sorted(xs)
    print(
        f"{name}: n={len(xs)} mean_ms={statistics.fmean(xs):.3f} "
        f"median_ms={statistics.median(xs):.3f} p95_ms={ys[int(len(xs)*0.95)-1]:.3f} "
        f"min_ms={ys[0]:.3f} max_ms={ys[-1]:.3f}"
    )

summary("default", default_times)
summary("forced_rgb", forced_times)
print(
    "delta_mean_ms(forced-default)="
    f"{statistics.fmean(forced_times) - statistics.fmean(default_times):.3f}"
)
PY
