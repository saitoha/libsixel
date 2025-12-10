#!/bin/sh
# lsqa_regression.sh - Quality regression guard for assessment/lsqa.
#
# The thresholds are intentionally strict (MS-SSIM >= 0.98, PSNR_Y >= 40dB)
# because the inputs represent lightly processed photographic and synthetic
# samples. These values allow minor encoder noise while still catching obvious
# regressions or decoder crashes.
# Sample set overview:
#   - formats/: RGB PNG, grayscale JPEG, palette PNG, RGB WebP
#   - resolutions/: 64x64 up to 1920x1080 with landscape and portrait layouts
#   - corrupted/: truncated PNG and noisy JPEG headers for robustness checks
#
# The script compares live metrics to a checked-in baseline and enforces
# per-case minimums. It emits a compact CSV so CI artifacts remain readable.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

export MS_SSIM_FLOOR=0.98
export PSNR_FLOOR=40.0
export BASELINE_DIR="$(dirname "$0")/data/baseline"
export INPUT_ROOT="$(dirname "$0")/data"
export ARTIFACT_ROOT="${ARTIFACT_ROOT:-$(pwd)/tests/_artifacts}"
export CSV_REPORT="${ARTIFACT_ROOT}/lsqa_resolutions.csv"
export SEED=${LSQA_SEED:-2024}

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
build_root=${TOP_BUILDDIR:-${script_dir}/..}
lsqa_bin_default="${build_root}/assessment/lsqa"

if [ ! -x "${lsqa_bin_default}" ] && [ -x "${build_root}/lsqa" ]; then
    lsqa_bin_default="${build_root}/lsqa"
fi

if [ ! -x "${lsqa_bin_default}" ]; then
    printf 'lsqa binary not found. looked for %s and %s\n' \
        "${build_root}/assessment/lsqa" "${build_root}/lsqa" >&2
    exit 1
fi

export LSQA_BIN=${LSQA_BIN:-${lsqa_bin_default}}

mkdir -p "${ARTIFACT_ROOT}"

# Prefer the explicit python3 interpreter when the generic `python`
# shim is unavailable (e.g., on FreeBSD images). Using a common helper
# keeps the shebang POSIX-compliant while still locating a functional
# Python runtime for the regression harness.
python_bin="${PYTHON:-python}"
if ! command -v "${python_bin}" >/dev/null 2>&1; then
    python_bin="python3"
fi

"${python_bin}" - <<PY
import json
import math
import os
import subprocess
import sys

ms_floor = float(os.environ.get("MS_SSIM_FLOOR", "0.98"))
psnr_floor = float(os.environ.get("PSNR_FLOOR", "40.0"))
base_dir = os.environ["BASELINE_DIR"]
input_root = os.environ["INPUT_ROOT"]
artifact_root = os.environ["ARTIFACT_ROOT"]
csv_report = os.environ["CSV_REPORT"]
seed = int(os.environ.get("SEED", "2024"))

cases = []
formats_dir = os.path.join(input_root, "inputs", "formats")
for entry in sorted(os.listdir(formats_dir)):
    cases.append((os.path.join(formats_dir, entry), entry))

res_dir = os.path.join(input_root, "resolutions")
for entry in sorted(os.listdir(res_dir)):
    cases.append((os.path.join(res_dir, entry), entry))

corrupted_dir = os.path.join(input_root, "corrupted")
corrupted = [os.path.join(corrupted_dir, name)
             for name in sorted(os.listdir(corrupted_dir))]

# Palette sources rely on indexed color and may omit MS-SSIM in the report.
custom_floors = {
    "palette.png": (0.0, psnr_floor),
}


def run_lsqa(path):
    env = os.environ.copy()
    env["LSQA_RANDOM_SEED"] = str(seed)
    proc = subprocess.run([
        os.environ["LSQA_BIN"],
        path,
        path,
    ], check=False, capture_output=True, text=True, env=env)
    return proc.returncode, proc.stdout, proc.stderr


def parse_metrics(text):
    data = json.loads(text)
    quality = data.get("quality", {})
    ms = quality.get("MS-SSIM", 0.0)
    psnr = quality.get("PSNR_Y", 0.0)
    ms = 0.0 if ms is None else float(ms)
    psnr = 0.0 if psnr is None else float(psnr)
    return ms, psnr


failures = []
rows = ["label,ms_ssim,psnr_y"]

for path, label in cases:
    code, out, err = run_lsqa(path)
    if code != 0:
        failures.append(f"{label}: assessment/lsqa returned {code}: {err.strip()}")
        continue
    try:
        ms, psnr = parse_metrics(out)
    except Exception as exc:  # noqa: BLE001
        failures.append(f"{label}: failed to parse metrics ({exc})")
        continue
    base_name = os.path.splitext(label)[0] + ".json"
    base_path = os.path.join(base_dir, base_name)
    if not os.path.exists(base_path):
        failures.append(f"{label}: baseline {base_name} missing")
        continue
    with open(base_path, "r", encoding="utf-8") as handle:
        base_data = json.load(handle).get("quality", {})
    base_ms_val = base_data.get("MS-SSIM", 0.0)
    base_psnr_val = base_data.get("PSNR_Y", 0.0)
    base_ms = 0.0 if base_ms_val is None else float(base_ms_val)
    base_psnr = 0.0 if base_psnr_val is None else float(base_psnr_val)
    floor_ms, floor_psnr = custom_floors.get(label, (ms_floor, psnr_floor))
    if ms + 1e-6 < floor_ms:
        failures.append(f"{label}: MS-SSIM {ms:.6f} below floor {floor_ms}")
    if psnr + 1e-6 < floor_psnr:
        failures.append(
            f"{label}: PSNR_Y {psnr:.3f} below floor {floor_psnr}dB")
    if ms + 1e-6 < base_ms:
        failures.append(
            f"{label}: MS-SSIM {ms:.6f} regressed from baseline {base_ms:.6f}")
    if psnr + 1e-6 < base_psnr:
        failures.append(
            f"{label}: PSNR_Y {psnr:.3f} regressed from baseline {base_psnr:.3f}")
    rows.append(f"{label},{ms:.6f},{psnr:.3f}")

with open(csv_report, "w", encoding="utf-8") as handle:
    handle.write("\n".join(rows))

# Repeat a stability check to monitor variance on the high-entropy palette
# case. A stable encoder should keep variance near zero when the seed is
# pinned; a jump indicates nondeterminism or state leakage.
repeat_label = "palette.png"
repeat_path = os.path.join(formats_dir, repeat_label)
repeats = []
for _ in range(5):
    code, out, err = run_lsqa(repeat_path)
    if code != 0:
        failures.append(f"{repeat_label}: repeat run failed ({code}) {err.strip()}")
        break
    ms, psnr = parse_metrics(out)
    repeats.append((ms, psnr))
if repeats:
    avg_ms = sum(v[0] for v in repeats) / len(repeats)
    avg_psnr = sum(v[1] for v in repeats) / len(repeats)
    var_ms = sum((v[0] - avg_ms) ** 2 for v in repeats) / len(repeats)
    var_psnr = sum((v[1] - avg_psnr) ** 2 for v in repeats) / len(repeats)
    if var_ms > 1e-6:
        failures.append(
            f"{repeat_label}: MS-SSIM variance {var_ms:.2e} exceeds 1e-6")
    if var_psnr > 1e-3:
        failures.append(
            f"{repeat_label}: PSNR_Y variance {var_psnr:.2e} exceeds 1e-3")

# Corrupted inputs must not crash; a non-zero exit code paired with stderr is
# acceptable as long as the process returns control.
for path in corrupted:
    code, out, err = run_lsqa(path)
    if code == 0:
        try:
            ms, psnr = parse_metrics(out)
        except Exception:  # noqa: BLE001
            failures.append(f"{path}: succeeded without parsable output")
            continue
        if ms < 0.5 or psnr < 10:
            failures.append(f"{path}: low quality accepted unexpectedly")
    else:
        if not err.strip():
            failures.append(f"{path}: failed without diagnostic output")

if failures:
    sys.stderr.write("\n".join(failures) + "\n")
    sys.exit(1)

print("lsqa regression suite passed; CSV stored at", csv_report)
PY
