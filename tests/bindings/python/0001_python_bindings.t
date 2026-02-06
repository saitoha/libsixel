#!/bin/sh
# TAP smoke test for Python bindings. The test prefers the prebuilt wheel
# under python-wheel/dist when available and falls back to the in-tree Python
# sources otherwise.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/python/common.sh"

python_prepare "${ARTIFACT_LOCAL_DIR}"
set -v

case_id=1
skip_code=200
skip_reason=""

if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${ARTIFACT_LOCAL_DIR}/venv"
    if python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_pass ${case_id} "installs wheel from python-wheel/dist"
    else
        tap_fail ${case_id} "wheel installation failed"
    fi
else
    python_output=$(env ${python_in_tree_loader_env} \
       PYTHONPATH="${python_in_tree_pythonpath}" \
       LIBSIXEL_LIBDIR="${python_lib_dir}" \
       "${run_python}" - 2>&1 <<'PY'
try:
    import libsixel
    from libsixel import encoder, decoder

    encoder.Encoder
    decoder.Decoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY
    )
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "imports in-tree python modules"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "failed to import in-tree python modules"
    fi
fi

case_id=$((case_id + 1))
verify_script="${ARTIFACT_LOCAL_DIR}/verify-bindings.py"
cat >"${verify_script}" <<'PY'
import ctypes.util
import glob
import os
import pathlib
import sys
import traceback

SKIP_CODE = int(os.environ.get("SKIP_CODE", "200"))


def _prefer_build_library(name, original_find):
    libdir = os.environ.get("LIBSIXEL_LIBDIR")
    if libdir:
        prefixes = ["lib", ""]
        suffixes = [".so", ".dylib", ".dll"]

        for prefix in prefixes:
            for suffix in suffixes:
                pattern = os.path.join(libdir,
                                       f"{prefix}{name}*{suffix}")
                matches = sorted(glob.glob(pattern))
                if matches:
                    return matches[0]

    return original_find(name)


def main():
    ctypes.util.find_library = (
        lambda name, _orig=ctypes.util.find_library:
        _prefer_build_library(name, _orig)
    )

try:
    from libsixel import SIXEL_PIXELFORMAT_RGB888
    from libsixel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

    root = pathlib.Path(__file__).parent
    output = root / "sample.six"

    pixels = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    # Exercise repeated encode paths to catch frame refcount regressions.
    for _ in range(2):
        encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

    if not output.exists():
        raise SystemExit("missing sixel output")
    if output.stat().st_size == 0:
        raise SystemExit("empty sixel output")

    print("encode succeeded")
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY

python_env="${run_python}"
libdir="${python_lib_dir}"

if [ -z "${libdir}" ]; then
    tap_fail ${case_id} "could not locate libsixel build output"
    exit ${tap_status}
fi

export LIBSIXEL_LIBDIR="${libdir}"

if [ -n "${skip_reason}" ]; then
    printf 'ok %s - %s # SKIP %s\n' \
        "${case_id}" \
        "encodes image via in-tree modules" \
        "${skip_reason}"
    exit ${status}
fi

if [ "${use_wheel}" -eq 1 ]; then
    python_output=$(env ${python_wheel_loader_env} \
       PYTHONPATH="${python_wheel_trace_pythonpath}" \
        "${python_env}" "${verify_script}" 2>&1)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "encodes image via wheel"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "python wheel round-trip failed"
    fi
else
    python_output=$(env ${python_in_tree_loader_env} \
       PYTHONPATH="${python_in_tree_trace_pythonpath}" \
       "${python_env}" "${verify_script}" 2>&1)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "encodes image via in-tree modules"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "python in-tree round-trip failed"
    fi
fi

tap_plan 2
exit ${tap_status}
