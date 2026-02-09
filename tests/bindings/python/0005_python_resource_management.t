#!/bin/sh
# Ensure resource cleanup for Python encode/decode workflows.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if [ "${ENABLE_PYTHON:-0}" != "1" ]; then
    skip_all "python bindings are disabled in this build"
fi

if [ -z "${SIXEL_TEST_PYTHON_VENV:-}" ] \
   || [ ! -x "${SIXEL_TEST_PYTHON_VENV}/bin/python" ]; then
    skip_all "python wheel test environment is unavailable"
fi

run_python="${SIXEL_TEST_PYTHON_VENV}/bin/python"
libdir="${LIBSIXEL_LIBDIR:-${TOP_BUILDDIR}/src/.libs}"
if [ ! -d "${libdir}" ]; then
    libdir="${TOP_BUILDDIR}/src"
fi

python_skip_on_load_error() {
    status=$1
    log_text=$2

    if [ "${status}" -eq 0 ]; then
        return 0
    fi

    marker=$(printf '%s' "${log_text}" | grep -m1 '^SKIP_LIBSIXEL_LOAD:' || true)
    if [ -n "${marker}" ]; then
        tap_skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"
    fi
}

case_id=1
source_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
work_dir="${ARTIFACT_LOCAL_DIR}/work"

python_output=$(env \
    LIBSIXEL_LIBDIR="${libdir}" \
    LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
    "${run_python}" - "${source_image}" "${work_dir}" 2>&1 <<'PY'
import gc
import pathlib
import sys
import time
import warnings
from typing import Tuple

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_WIDTH,
        SIXEL_OPTFLAG_HEIGHT,
    )
    from libsixel_wheel.encoder import Encoder
    from libsixel_wheel.decoder import Decoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)


def encode_large(source: pathlib.Path, target: pathlib.Path) -> int:
    with Encoder() as encoder:
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.setopt(SIXEL_OPTFLAG_WIDTH, "800")
        encoder.setopt(SIXEL_OPTFLAG_HEIGHT, "600")
        encoder.encode(str(source))
    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("encoder output missing or empty")
    return target.stat().st_size


def decode_large(source: pathlib.Path, target: pathlib.Path) -> Tuple[int, int]:
    with Decoder() as decoder:
        decoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
        decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        decoder.decode(str(source))
    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("decoder output missing or empty")
    header = target.read_bytes()[:24]
    if len(header) < 24:
        raise SystemExit("decoded PNG header too small")
    return int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")


def remove_path(path: pathlib.Path, deadline: float = 15.0) -> None:
    limit = time.monotonic() + deadline
    while True:
        gc.collect()
        try:
            path.unlink()
            return
        except FileNotFoundError:
            return
        except PermissionError:
            if time.monotonic() >= limit:
                raise SystemExit(f"failed to remove {path.name}")
            time.sleep(0.2)


source = pathlib.Path(sys.argv[1])
workdir = pathlib.Path(sys.argv[2])
workdir.mkdir(parents=True, exist_ok=True)

sixel_path = workdir / "large.six"
decoded_png = workdir / "roundtrip.png"

with warnings.catch_warnings():
    warnings.simplefilter("error", ResourceWarning)
    size = encode_large(source, sixel_path)
    width, height = decode_large(sixel_path, decoded_png)

gc.collect()
time.sleep(0.5)
remove_path(sixel_path)
remove_path(decoded_png)

if sixel_path.exists() or decoded_png.exists():
    raise SystemExit("output files persist after deletion")

print(f"encoded {size} bytes, decoded {width}x{height}, resources cleaned")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
if [ "${python_status}" -eq 0 ]; then
    tap_pass ${case_id} "large image roundtrip via wheel frees resources"
else
    python_skip_on_load_error "${python_status}" "${python_output}"
    tap_fail ${case_id} "resource test via wheel failed"
fi

tap_plan 1
exit 0
